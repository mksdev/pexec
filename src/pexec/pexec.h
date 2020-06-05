//
// Created by Michal Němec on 05/06/2020.
//

#ifndef FORK_WATCH_PEXEC_H
#define FORK_WATCH_PEXEC_H

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

#include "proc_status.h"
#include "argument_parser.h"

namespace pexec {

int fd_set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int fd_set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_CLOEXEC;
    return fcntl(fd, F_SETFL, flags);
}

int fd_unset_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

#if defined __APPLE__
typedef void (*__sighandler_t) (int);

int pipe2(int* p, int flags) {
    int ret = 0;
    if((ret = pipe(p)) < 0) return -1;
    if(flags == 0) return ret;

    if((flags & O_NONBLOCK) == O_NONBLOCK) {
        if(fd_set_nonblock(p[0]) < 0) return -1;
        if(fd_set_nonblock(p[1]) < 0) return -1;
    }
    if((flags & O_CLOEXEC) == O_CLOEXEC) {
        if(fd_set_cloexec(p[0]) < 0) return -1;
        if(fd_set_cloexec(p[1]) < 0) return -1;
    }
    return ret;
}
#endif

void close_fd(int* fd) {
    if(*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

void close_pipe(int* pipe) {
    close_fd(&pipe[0]);
    close_fd(&pipe[1]);
}

std::vector<char *> arg2argc(const std::vector<std::string>& args) {
    std::vector<char*> args_c;
    args_c.reserve(args.size()+1);
    for(auto& arg : args) {
        args_c.emplace_back((char*)arg.c_str());
    }
    args_c.emplace_back(nullptr);
    return args_c;
}

using fd_callback = std::function<void(const char* buffer, std::size_t size)>;
using fd_state_callback = std::function<void(proc_status::state, proc_status&)>;

int pipe_signal[2] = {-1, -1};

void signal_handler(int sig) {
    ::write(pipe_signal[1], (void *)&sig, sizeof(sig));
}

enum class status : int {
    SUCESSS,
    SIGACTION_SET_ERROR,
    SIGACTION_RESET_ERROR,
    STDIN_PIPE_ERROR,
    STDERR_PIPE_ERROR,
    STDOUT_PIPE_ERROR,
    FORK_ERROR,
    ARG_PARSE_ERROR,
    ARG_PARSE_C_ERROR,
    SIGNAL_PIPE_ERROR,
    WATCH_PIPE_ERROR,
    SIG_BLOCK_ERROR,
    SIG_UNBLOCK_ERROR,
    SELECT_ERROR,
    STDOUT_PIPE_READ_ERROR,
    STDERR_PIPE_READ_ERROR,
    SIGNAL_PIPE_READ_ERROR,
    WATCH_PIPE_READ_ERROR,
    WAITPID_ERROR,
    FORK_EXEC_ERROR, //100
    FORK_STDIN_NONBLOCK_ERROR, //101
    FORK_STDOUT_NONBLOCK_ERROR, //102
    FORK_STDERR_NONBLOCK_ERROR, //103
    FORK_DUP2_STDIN_ERROR, //104
    FORK_DUP2_STDOUT_ERROR, //105
    FORK_DUP2_STDERR_ERROR, //106
};


std::string status2string(enum status err) {
    switch (err) {
        case status::SUCESSS: return "SUCESSS";
        case status::SIGACTION_SET_ERROR: return "SIGACTION_SET_ERROR";
        case status::SIGACTION_RESET_ERROR: return "SIGACTION_RESET_ERROR";
        case status::STDIN_PIPE_ERROR: return "STDIN_PIPE_ERROR";
        case status::STDERR_PIPE_ERROR: return "STDERR_PIPE_ERROR";
        case status::STDOUT_PIPE_ERROR: return "STDOUT_PIPE_ERROR";
        case status::FORK_ERROR: return "FORK_ERROR";
        case status::ARG_PARSE_ERROR: return "ARG_PARSE_ERROR";
        case status::ARG_PARSE_C_ERROR: return "ARG_PARSE_C_ERROR";
        case status::SIGNAL_PIPE_ERROR: return "SIGNAL_PIPE_ERROR";
        case status::WATCH_PIPE_ERROR: return "WATCH_PIPE_ERROR";
        case status::SIG_BLOCK_ERROR: return "SIG_BLOCK_ERROR";
        case status::SIG_UNBLOCK_ERROR: return "SIG_UNBLOCK_ERROR";
        case status::SELECT_ERROR: return "SELECT_ERROR";
        case status::STDOUT_PIPE_READ_ERROR: return "STDOUT_PIPE_READ_ERROR";
        case status::STDERR_PIPE_READ_ERROR: return "STDERR_PIPE_READ_ERROR";
        case status::SIGNAL_PIPE_READ_ERROR: return "SIGNAL_PIPE_READ_ERROR";
        case status::WATCH_PIPE_READ_ERROR: return "WATCH_PIPE_READ_ERROR";
        case status::WAITPID_ERROR: return "WAITPID_ERROR";
        case status::FORK_EXEC_ERROR: return "FORK_EXEC_ERROR";
        case status::FORK_STDIN_NONBLOCK_ERROR: return "FORK_STDIN_NONBLOCK_ERROR";
        case status::FORK_STDOUT_NONBLOCK_ERROR: return "FORK_STDOUT_NONBLOCK_ERROR";
        case status::FORK_STDERR_NONBLOCK_ERROR: return "FORK_STDERR_NONBLOCK_ERROR";
        case status::FORK_DUP2_STDIN_ERROR: return "FORK_DUP2_STDIN_ERROR";
        case status::FORK_DUP2_STDOUT_ERROR: return "FORK_DUP2_STDOUT_ERROR";
        case status::FORK_DUP2_STDERR_ERROR: return "FORK_DUP2_STDERR_ERROR";
    }

}

using error_status = std::function<void(status)>;

template<int BUFFER_SIZE = 1024>
class pexec {

    std::array<char, BUFFER_SIZE> read_buffer{};

    fd_callback stdout_cb_ = [&](const char* data, std::size_t len){};
    fd_callback stderr_cb_ = [&](const char* data, std::size_t len){};
    fd_state_callback state_cb_ = [&](proc_status::state state, proc_status& proc) {};
    error_status error_cb_ = [](enum status s){};

    status state_ = status::SUCESSS;

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

public:
    void prepare_signal_set() {
        sigemptyset(&signal_set_);
        sigaddset(&signal_set_, SIGCHLD);
    }

    void close_fork_pipes() {
        close_pipe(pipe_stdin_);
        close_pipe(pipe_stdout_);
        close_pipe(pipe_stderr_);
        close_pipe(pipe_signal);
        close_pipe(pipe_close_watch_);
    }

    bool prepare_fork_pipes() {
        if(pipe2(pipe_stdin_, O_NONBLOCK) < 0) {
            process_error(status::STDIN_PIPE_ERROR);
            close_fork_pipes();
            return false;
        }
        if(pipe2(pipe_stdout_, O_NONBLOCK) < 0) {
            process_error(status::STDOUT_PIPE_ERROR);
            close_fork_pipes();
            return false;
        }
        if(pipe2(pipe_stderr_, O_NONBLOCK) < 0) {
            process_error(status::STDERR_PIPE_ERROR);
            close_fork_pipes();
            return false;
        }
        if(pipe2(pipe_signal, O_CLOEXEC | O_NONBLOCK) < 0) {
            process_error(status::SIGNAL_PIPE_ERROR);
            close_fork_pipes();
            return false;
        }
        if(pipe2(pipe_close_watch_, O_CLOEXEC | O_NONBLOCK) < 0) {
            process_error(status::WATCH_PIPE_ERROR);
            close_fork_pipes();
            return false;
        }
        return true;
    }

    bool prepare_args() {
        // parse program agrumets to the array
        args_ = util::str2arg(spawn_process_arg_);
        if(args_.empty()) {
            process_error(status::ARG_PARSE_ERROR);
            close_fork_pipes();
            return false;
        }
        args_c_ = arg2argc(args_);
        if(args_c_.size() < 2) {
            process_error(status::ARG_PARSE_C_ERROR);
            close_fork_pipes();
            return false;
        }
        return true;
    }

    bool set_sigchld_signal_handler() {
        // set signal handler for SIGCHILD
        sa_.sa_handler = signal_handler;
        sigemptyset(&sa_.sa_mask);
        sa_.sa_flags = 0;
        {
            // set signal handler, save previous
            auto sigaction_ret = ::sigaction(SIGCHLD, &sa_, &sa_prev_);
            if(sigaction_ret == -1) {
                process_error(status::SIGACTION_SET_ERROR);
                close_fork_pipes();
                return false;
            }
        }
        return true;
    }

    bool reset_sigchld_signal_handler() {
        //reset SIGCHLD signal handler
        auto sigaction_ret = ::sigaction(SIGCHLD, &sa_prev_, nullptr);
        if(sigaction_ret == -1) {
            process_error(status::SIGACTION_RESET_ERROR);
            return false;
        }
        return true;
    }

    void process_error(enum status st) {
        error_cb_(st);
        state_ = st;
    }

    void block_sigchld() {
        if(sigprocmask(SIG_BLOCK, &signal_set_, nullptr) == -1) {
            process_error(status::SIG_BLOCK_ERROR);
        }
    }

    void unblock_sigchld() {
        if(sigprocmask(SIG_UNBLOCK, &signal_set_, nullptr) == -1) {
            process_error(status::SIG_UNBLOCK_ERROR);
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
                process_error(status::WAITPID_ERROR);
            }
        } while(errno == EINTR);
        errno = save_errno;

        proc_.update_status(status_);
        state_cb_(proc_status::state::SIGNALED, proc_);

        if (!proc_.running) {
            proc_.stdin_fd = -1;
            proc_killed_ = true;
            handle_proc_return_code();
        }

        unblock_sigchld();
    }


    void handle_proc_return_code() {
        switch (proc_.return_code) {
            case 100 : {
                process_error(status::FORK_EXEC_ERROR);
                break;
            }
            case 101 : {
                process_error(status::FORK_STDIN_NONBLOCK_ERROR);
                break;
            }
            case 102 : {
                process_error(status::FORK_STDOUT_NONBLOCK_ERROR);
                break;
            }
            case 103 : {
                process_error(status::FORK_STDERR_NONBLOCK_ERROR);
                break;
            }
            case 104 : {
                process_error(status::FORK_DUP2_STDIN_ERROR);
                break;
            }
            case 105 : {
                process_error(status::FORK_DUP2_STDOUT_ERROR);
                break;
            }
            case 106 : {
                process_error(status::FORK_DUP2_STDERR_ERROR);
                break;
            }
        }
    }

    void loop_io() {

        ssize_t rc;
        fd_set read_fds{};

        // file descriptors we will watch with select
        std::array<int, 4> watch_fds{
                pipe_close_watch_[0],
                pipe_stdout_[0],
                pipe_stderr_[0],
                pipe_signal[0]
        };

        auto max_fd = *std::max_element(watch_fds.begin(), watch_fds.end());

        proc_killed_ = false;
        user_stopped_ = false;
        status_ = 0;

        while(true) {

            bool failed = false;
            int save_errno = errno;
            do {
                FD_ZERO(&read_fds);
                for(auto&& fd : watch_fds) {
                    FD_SET(fd, &read_fds);
                }
                int select_ret;
                errno = 0;
                if ((select_ret = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr)) < 0) {
                    if(errno != EINTR) {
                        process_error(status::SELECT_ERROR);
                        failed = true;
                        break;
                    }
                }
            } while(errno == EINTR);
            errno = save_errno;

            if(failed) {
                break;
            }

            if (FD_ISSET(pipe_stdout_[0], &read_fds)) {
                if ((rc = read(pipe_stdout_[0], read_buffer.data(), read_buffer.size() - 1)) < 0) {
                    process_error(status::STDOUT_PIPE_READ_ERROR);
                    break;
                }
                if (rc == 0) continue;
                read_buffer[rc] = 0;
                stdout_cb_(read_buffer.data(), rc);
            }

            if (FD_ISSET(pipe_stderr_[0], &read_fds)) {
                if ((rc = read(pipe_stderr_[0], read_buffer.data(), read_buffer.size() - 1)) < 0) {
                    process_error(status::STDERR_PIPE_READ_ERROR);
                    break;
                }
                if (rc == 0) continue;
                read_buffer[rc] = 0;
                stderr_cb_(read_buffer.data(), rc);
            }

            if (FD_ISSET(pipe_signal[0], &read_fds)) {
                int signal = 0;

                int read_from = 0;
                int want_read = sizeof(signal);
                do {
                    if ((rc = read(pipe_signal[0], static_cast<void*>((char*)(&signal) + read_from), want_read)) < 0) {
                        process_error(status::SIGNAL_PIPE_READ_ERROR);
                        break;
                    }
                    read_from += rc;
                    want_read -= rc;
                } while(want_read != 0);
                if(want_read != 0) {
                    break;
                }
                if(signal == SIGCHLD) {
                    handle_sigchld();
                }
                if(proc_killed_) {
                    break;
                }
            }

            if (FD_ISSET(pipe_close_watch_[0], &read_fds)) {
                user_stopped_ = true;
                do {
                    if ((rc = read(pipe_close_watch_[0], read_buffer.data(), read_buffer.size() - 1)) < 0) {
                        if(errno != EAGAIN) {
                            process_error(status::WATCH_PIPE_READ_ERROR);
                        }
                        break;
                    }
                    if (rc == 0) continue;
                }while(true);
                break;
            }
        }
    }

    void loop_rest_io() {
        ssize_t rc;
        // reading rest of the pipe_stdout buffer
        do {
            if ((rc = read(pipe_stdout_[0], read_buffer.data(), read_buffer.size() - 1)) < 0) {
                if(errno != EAGAIN) {
                    process_error(status::STDOUT_PIPE_READ_ERROR);
                }
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stdout_cb_(read_buffer.data(), rc);
        }while(true);

        // reading rest of the pipe_stderr buffer
        do {
            if ((rc = read(pipe_stderr_[0], read_buffer.data(), read_buffer.size() - 1)) < 0) {
                if(errno != EAGAIN) {
                    process_error(status::STDERR_PIPE_READ_ERROR);
                }
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stderr_cb_(read_buffer.data(), rc);
        }while(true);
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

    void set_error_cb(error_status cb) {
        error_cb_ = std::move(cb);
    }

    bool spawn_proc() {
        proc_pid_ = fork();
        if(proc_pid_ == -1) {
            process_error(status::FORK_ERROR);
            close_fork_pipes();
            return false;
        }
        if(proc_pid_ == 0) {
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

    void exec(const std::string& spawn_arg) noexcept {
        spawn_process_arg_ = spawn_arg;
        state_ = status::SUCESSS;
        if(!prepare_args()) {
            return;
        }
        // create signal blocking
        prepare_signal_set();
        if(!prepare_fork_pipes()) {
            return;
        }
        if(!set_sigchld_signal_handler()) {
            return;
        }

        // spawn process
        block_sigchld();
        auto spawned = spawn_proc();
        unblock_sigchld();
        if(!spawned) {
            return;
        }

        // process structure
        proc_.pid = proc_pid_;
        proc_.stdin_fd = pipe_stdin_[1];
        proc_.running = true;
        proc_.user_stop_fd = pipe_close_watch_[1];

        state_cb_(proc_status::state::STARTED, proc_);

        loop_io();

        reset_sigchld_signal_handler();

        if(user_stopped_) {
            proc_.user_stop_fd = -1;
            state_cb_(proc_status::state::USER_STOPPED, proc_);
        } else {
            loop_rest_io();
            state_cb_(proc_status::state::STOPPED, proc_);
        }

        close_fork_pipes();
    }

};


struct exec_status {
    std::string proc_out;
    std::string proc_err;

    proc_status proc;
    std::vector<status> err;
};

exec_status exec(const std::string& arg) {

    exec_status ret{};

    std::ostringstream stdout_oss;
    std::ostringstream stderr_oss;

    pexec<> proc;
    proc.set_error_cb([&](status error){
        ret.err.push_back(error);
    });
    proc.set_stdout_cb([&](const char* data, std::size_t len){
        stdout_oss << data;
    });
    proc.set_stderr_cb([&](const char* data, std::size_t len){
        stderr_oss << data;
    });
    proc.set_state_cb([&](proc_status::state state, proc_status& proc) {
        ret.proc = proc;
    });
    proc.exec(arg);

    ret.proc_out = stdout_oss.str();
    ret.proc_err = stderr_oss.str();

    return ret;
}


}

#endif //FORK_WATCH_PEXEC_H
