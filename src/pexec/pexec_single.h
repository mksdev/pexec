//
// Created by Michal Němec on 05/06/2020.
//

#ifndef PEXEC_PEXEC_SINGLE_H
#define PEXEC_PEXEC_SINGLE_H

#include <iostream>
#include <array>
#include <vector>
#include <cstdio>
#include <unistd.h>

#include <zconf.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>
#include <sstream>

#include "event/select_event.h"
#include "proc_status.h"
#include "argument_parser.h"
#include "error.h"
#include "util.h"

namespace pexec {

using fd_callback = std::function<void(const char* buffer, std::size_t size)>;
using fd_state_callback = std::function<void(proc_status::state, proc_status&)>;

enum type {
    BLOCKING, NONBLOCKING
};

struct pexec_fds{
    int stdout_read_fd;
    int stderr_read_fd;
    int stdin_write_fd;
    int watch_close_write_fd;
};

class pexec_multi;
class pexec_multi_handle;


template<int BUFFER_SIZE = 1024>
class pexec {

    type type_ = type::BLOCKING;

    std::array<char, BUFFER_SIZE> read_buffer{};

    fd_callback stdout_cb_ = [&](const char* data, std::size_t len){};
    fd_callback stderr_cb_ = [&](const char* data, std::size_t len){};
    fd_state_callback state_cb_ = [&](proc_status::state state, proc_status& proc) {};
    error_status_cb error_cb_ = [](enum error s){};

    error state_ = error::NO_ERROR;

    sigset_t signal_set_{};
    // create communication pipes
    int pipe_stdin_[2] = {-1, -1};
    int pipe_stdout_[2] = {-1, -1};
    int pipe_stderr_[2] = {-1, -1};

    int pipe_close_watch_[2] = {-1, -1};

    std::vector<std::string> args_;
    std::vector<char*> args_c_;

    struct sigaction sa_{};
    struct sigaction sa_prev_{};

    proc_status proc_{};

    bool proc_killed_ = false;
    bool user_stopped_ = false;
    int status_ = 0;

    pid_t proc_pid_ = 0;

    std::string spawn_process_arg_;

    void prepare_signal_set() {
        sigemptyset(&signal_set_);
        sigaddset(&signal_set_, SIGCHLD);
    }

    void close_fork_pipes() {
        close_pipe(pipe_stdin_);
        close_pipe(pipe_stdout_);
        close_pipe(pipe_stderr_);
        if(type_ == type::BLOCKING) {
            close_pipe(sigchld_blocking_pipe_signal);
        }
        close_pipe(pipe_close_watch_);
    }

    bool prepare_fork_pipes() {
        if(pipe2(pipe_stdin_, O_NONBLOCK) < 0) {
            process_error(error::STDIN_PIPE_ERROR);
            fail_stopped();
            return false;
        }
        if(pipe2(pipe_stdout_, O_NONBLOCK) < 0) {
            process_error(error::STDOUT_PIPE_ERROR);
            fail_stopped();
            return false;
        }
        if(pipe2(pipe_stderr_, O_NONBLOCK) < 0) {
            process_error(error::STDERR_PIPE_ERROR);
            fail_stopped();
            return false;
        }

        if(type_ == type::BLOCKING) {
            if(pipe2(sigchld_blocking_pipe_signal, O_CLOEXEC | O_NONBLOCK) < 0) {
                process_error(error::SIGNAL_PIPE_ERROR);
                fail_stopped();
                return false;
            }
        }

        if(pipe2(pipe_close_watch_, O_CLOEXEC | O_NONBLOCK) < 0) {
            process_error(error::WATCH_PIPE_ERROR);
            fail_stopped();
            return false;
        }
        return true;
    }

    bool prepare_args() {
        // parse program agrumets to the array
        args_ = util::str2arg(spawn_process_arg_);
        if(args_.empty()) {
            process_error(error::ARG_PARSE_ERROR);
            fail_stopped();
            return false;
        }
        args_c_ = arg2argc(args_);
        if(args_c_.size() < 2) {
            process_error(error::ARG_PARSE_C_ERROR);
            fail_stopped();
            return false;
        }
        return true;
    }

    bool set_sigchld_signal_handler() {
        // set signal handler for SIGCHILD
        sa_.sa_handler = sigchld_blocking_signal_handler;
        sigemptyset(&sa_.sa_mask);
        sa_.sa_flags = 0;
        {
            // set signal handler, save previous
            auto sigaction_ret = ::sigaction(SIGCHLD, &sa_, &sa_prev_);
            if(sigaction_ret == -1) {
                process_error(error::SIGACTION_SET_ERROR);
                fail_stopped();
                return false;
            }
        }
        return true;
    }

    bool reset_sigchld_signal_handler() {
        //reset SIGCHLD signal handler
        auto sigaction_ret = ::sigaction(SIGCHLD, &sa_prev_, nullptr);
        if(sigaction_ret == -1) {
            process_error(error::SIGACTION_RESET_ERROR);
            return false;
        }
        return true;
    }

    void process_error(enum error st) {
        error_cb_(st);
        state_ = st;
    }

    void block_sigchld() {
        if(sigprocmask(SIG_BLOCK, &signal_set_, nullptr) == -1) {
            process_error(error::SIG_BLOCK_ERROR);
        }
    }

    void unblock_sigchld() {
        if(sigprocmask(SIG_UNBLOCK, &signal_set_, nullptr) == -1) {
            process_error(error::SIG_UNBLOCK_ERROR);
        }
    }

    void handle_sigchld() {
        block_sigchld();
        pid_t ret;
        int save_errno = errno;
        do {
            status_ = 0;
            errno = 0;
            if((ret = ::waitpid(proc_pid_, &status_, WNOHANG)) <= 0) {
                process_error(error::WAITPID_ERROR);
            }
        } while(errno == EINTR);
        errno = save_errno;
        unblock_sigchld();

        update_status(status_);
    }


    void handle_proc_return_code() {
        switch (proc_.return_code) {
            case 100 : {
                process_error(error::FORK_EXEC_ERROR);
                break;
            }
            case 101 : {
                process_error(error::FORK_STDIN_NONBLOCK_ERROR);
                break;
            }
            case 102 : {
                process_error(error::FORK_STDOUT_NONBLOCK_ERROR);
                break;
            }
            case 103 : {
                process_error(error::FORK_STDERR_NONBLOCK_ERROR);
                break;
            }
            case 104 : {
                process_error(error::FORK_DUP2_STDIN_ERROR);
                break;
            }
            case 105 : {
                process_error(error::FORK_DUP2_STDOUT_ERROR);
                break;
            }
            case 106 : {
                process_error(error::FORK_DUP2_STDERR_ERROR);
                break;
            }
        }
    }

    void loop_io() {

        ssize_t rc;
        proc_killed_ = false;
        user_stopped_ = false;
        status_ = 0;

        select_event loop;
        loop.add_read_event(pipe_close_watch_[0], [&](int fd) {
            auto ret = read_close();
            if(ret == event_return::NOTHING) {
                loop.interrupt();
            }
            return ret;
        });
        loop.add_read_event(pipe_stdout_[0], [&](int fd){
            return read_stdout();
        });
        loop.add_read_event(pipe_stderr_[0], [&](int fd){
            return read_stderr();
        });
        loop.add_read_event(sigchld_blocking_pipe_signal[0], [&](int fd){
            int signal = 0;
            int read_from = 0;
            int want_read = sizeof(signal);
            do {
                if ((rc = read(sigchld_blocking_pipe_signal[0], static_cast<void*>((char*)(&signal) + read_from), want_read)) < 0) {
                    process_error(error::SIGNAL_PIPE_READ_ERROR);
                    break;
                }
                read_from += rc;
                want_read -= rc;
            } while(want_read != 0);
            if(want_read != 0) {
                return event_return::STOP_LOOP;
            }
            if(signal == SIGCHLD) {
                handle_sigchld();
            }
            if(proc_killed_) {
                return event_return::STOP_LOOP;
            }
            return event_return::NOTHING;
        });
        loop.on_error([&](){
            process_error(error::SELECT_ERROR);
        });
        loop.on_interrupt([&](){
            user_stopped_ = true;
            return event_return::STOP_LOOP;
        });

        loop.loop();
    }

    void read_std_rest(int fd, const fd_callback& cb, error throw_err) {
        ssize_t rc;
        // reading rest of the pipe_stderr buffer
        do {
            errno = 0;
            if ((rc = read(fd, read_buffer.data(), read_buffer.size() - 1)) < 0) {
                if(errno != EINTR && errno != EAGAIN) {
                    process_error(throw_err);
                }
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            cb(read_buffer.data(), rc);
        } while(errno == EINTR || rc == 0);
    }

    void loop_rest_io() {
        // process has stopped, but there might be some data left that we did not have time to read
        // file descriptors are in nonblocking mode and we can read it until (errno == EAGAIN) is returned
        read_std_rest(pipe_stdout_[0], stdout_cb_, error::STDOUT_PIPE_READ_REST_ERROR);
        read_std_rest(pipe_stderr_[0], stderr_cb_, error::STDERR_PIPE_READ_REST_ERROR);
    }

    bool spawn_proc() {
        proc_pid_ = fork();
        if(proc_pid_ == -1) {
            process_error(error::FORK_ERROR);
            fail_stopped();
            return false;
        }
        if(proc_pid_ == 0) {

            /*
            sigset_t ss{};
            sigemptyset(&ss);
            sigaddset(&ss, SIGCHLD);
            sigprocmask(SIG_UNBLOCK, &signal_set_, nullptr);
            ::signal(SIGCHLD, SIG_DFL);
            */

            if(fd_unset_nonblock(pipe_stdin_[0]) == -1) {
                exit(101);
            }
            if(fd_unset_nonblock(pipe_stdout_[1]) == -1) {
                exit(102);
            }
            if(fd_unset_nonblock(pipe_stderr_[1]) == -1) {
                exit(103);
            }
            //child
            if(dup2(pipe_stdin_[0], STDIN_FILENO) != STDIN_FILENO) {
                exit(104);
            }
            if(dup2(pipe_stdout_[1], STDOUT_FILENO) != STDOUT_FILENO) {
                exit(105);
            }
            if(dup2(pipe_stderr_[1], STDERR_FILENO) != STDERR_FILENO) {
                exit(106);
            }

            close_pipe(pipe_stdout_);
            close_pipe(pipe_stderr_);
            close_pipe(pipe_stdin_);

            //execute program
            execvp(args_c_[0], args_c_.data());
            //only when file in args_c[0] not found, should not happen
            exit(100);
        }
        return true;
    }

    void update_status(int status) {
        status_ = status;
        proc_.update_status(status);
        call_state(proc_status::state::SIGNALED);
        if (!proc_.running) {
            proc_.stdin_fd = -1;
            proc_killed_ = true;
            handle_proc_return_code();
            if(type_ == type::NONBLOCKING) {
                stopped();
            }
        }
    }

    void user_stopped() {
        user_stopped_ = true;
        proc_.user_stop_fd = -1;
        call_state(proc_status::state::USER_STOPPED);
        close_fork_pipes();
    }

    void fail_stopped() {
        call_state(proc_status::state::FAIL_STOPPED);
        close_fork_pipes();
    }

    void stopped() {
        loop_rest_io();
        call_state(proc_status::state::STOPPED);
        close_fork_pipes();
    }

    event_return read_close() {
        char c;
        ssize_t rc;
        int fd = pipe_close_watch_[0];
        do {
            errno = 0;
            if ((rc = ::read(fd, read_buffer.data(), read_buffer.size() - 1)) < 0) {
                if(errno != EINTR) {
                    process_error(error::WATCH_PIPE_READ_ERROR);
                    if(type_ == type::NONBLOCKING) {
                        fail_stopped();
                        return event_return::NOTHING;
                    }
                    return event_return::STOP_LOOP;
                }
            }
        } while(errno == EINTR);
        if(type_ == type::NONBLOCKING) {
            user_stopped();
        }
        return event_return::NOTHING;
    }

    event_return read_std(int fd, const fd_callback& cb, error throw_err) {
        ssize_t rc;
        do {
            errno = 0;
            if ((rc = ::read(fd, read_buffer.data(), read_buffer.size() - 1)) < 0) {
                if(errno != EINTR) {
                    process_error(throw_err);
                    if(type_ == type::NONBLOCKING) {
                        fail_stopped();
                        return event_return::NOTHING;
                    }
                    return event_return::STOP_LOOP;
                }
            }
        } while(errno == EINTR);
        if (rc == 0) {
            return event_return::SKIP_OTHER_FD;
        }
        if(rc == -1) {
            // errno == EAGAIN
            return event_return::SKIP_OTHER_FD;
        }
        read_buffer[rc] = 0;
        cb(read_buffer.data(), rc);
        return event_return::NOTHING;
    }

    event_return read_stdout() {
        return read_std(pipe_stdout_[0], stdout_cb_, error::STDOUT_PIPE_READ_ERROR);
    }

    event_return read_stderr() {
        return read_std(pipe_stderr_[0], stderr_cb_, error::STDERR_PIPE_READ_ERROR);
    }

    pexec_fds get_fds() {
        pexec_fds out{};
        out.stderr_read_fd = pipe_stderr_[0];
        out.stdout_read_fd = pipe_stdout_[0];
        out.stdin_write_fd = pipe_stdin_[1];
        out.watch_close_write_fd = pipe_close_watch_[0];
        return out;
    }

public:

    void set_type(type t) {
        type_ = t;
    }

    void set_stdout_cb(fd_callback cb) {
        stdout_cb_ = std::move(cb);
    }

    void set_stderr_cb(fd_callback cb) {
        stderr_cb_ = std::move(cb);
    }

    void set_state_cb(fd_state_callback cb) {
        state_cb_ = std::move(cb);
    }

    void set_error_cb(error_status_cb cb) {
        error_cb_ = std::move(cb);
    }

    void call_state(proc_status::state state) {
        state_cb_(state, proc_);
    }

    void exec(const std::string& spawn_arg) noexcept {
        spawn_process_arg_ = spawn_arg;
        state_ = error::NO_ERROR;
        if(!prepare_args()) {
            return;
        }
        // create signal blocking
        prepare_signal_set();
        if(!prepare_fork_pipes()) {
            return;
        }

        if(type_ == type::BLOCKING) {
            if(!set_sigchld_signal_handler()) {
                return;
            }
        }

        // spawn process
        //block_sigchld();
        auto spawned = spawn_proc();
        //unblock_sigchld();
        if(!spawned) {
            return;
        }

        // process structure
        proc_.pid = proc_pid_;
        proc_.stdin_fd = pipe_stdin_[1];
        proc_.running = true;
        proc_.user_stop_fd = pipe_close_watch_[1];

        call_state(proc_status::state::STARTED);

        if(type_ == type::BLOCKING) {
            loop_io();
            reset_sigchld_signal_handler();
            if(user_stopped_) {
                user_stopped();
            } else {
                stopped();
            }
        }
    }

    friend pexec_multi_handle;
    friend pexec_multi;
};


}

#endif //PEXEC_PEXEC_SINGLE_H
