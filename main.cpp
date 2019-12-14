#include <iostream>
#include <array>
#include <cstdio>
#include <unistd.h>

#include <zconf.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sstream>
#include "argument_parser.h"

int fd_set_nonblock(int fd) {
    return fcntl(fd, F_SETFL, O_NONBLOCK);
}

int fd_set_cloexec(int fd) {
    return fcntl(fd, F_SETFL, O_CLOEXEC);
}

#if defined __APPLE__
typedef void (*__sighandler_t) (int);

int pipe2(int* p, int flags) {
    int ret;
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

void close_pipe(int* pipe) {
    close(pipe[0]);
    close(pipe[1]);
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

struct proc_status {

    enum class state {
        STARTED, SIGNALED, STOPPED, USER_STOPPED
    };

    static std::string state2str(state state) noexcept {
        switch(state) {
            case state::STARTED: return "STARTED";
            case state::SIGNALED: return "SIGNALED";
            case state::STOPPED: return "STOPPED";
            case state::USER_STOPPED: return "STOPPED";
        }
    }

    int user_stop_fd = -1;

    pid_t pid = 0;
    int stdin_fd = -1;
    bool running = false;

    int wstatus = 0;

    bool exited = false;
    int return_code = 0;
    bool signaled = false;
    int signaled_signal = 0;
    bool generated_core_dump = false;
    bool stopped = false;
    bool stopped_signal = false;
    bool continued = false;

    void user_stop() const noexcept {
        const char c = '\0';
        assert(user_stop_fd != -1);
        write(user_stop_fd, &c, sizeof(c));
    }

    void update_status(int status) noexcept {
        wstatus = status;
        /**
        WIFEXITED(status)
            returns true if the child terminated normally, that is, by calling exit(3) or _exit(2), or by returning from main().
        WEXITSTATUS(status)
            returns the exit status of the child. This consists of the least significant 8 bits of the status argument that the child specified in a call to exit(3) or _exit(2) or as the argument for a return statement in main(). This macro should only be employed if WIFEXITED returned true.
        WIFSIGNALED(status)
            returns true if the child process was terminated by a signal.
        WTERMSIG(status)
            returns the number of the signal that caused the child process to terminate. This macro should only be employed if WIFSIGNALED returned true.
        WCOREDUMP(status)
            returns true if the child produced a core dump. This macro should only be employed if WIFSIGNALED returned true. This macro is not specified in POSIX.1-2001 and is not available on some UNIX implementations (e.g., AIX, SunOS). Only use this enclosed in #ifdef WCOREDUMP ... #endif.
        WIFSTOPPED(status)
            returns true if the child process was stopped by delivery of a signal; this is only possible if the call was done using WUNTRACED or when the child is being traced (see ptrace(2)).
        WSTOPSIG(status)
            returns the number of the signal which caused the child to stop. This macro should only be employed if WIFSTOPPED returned true.
        WIFCONTINUED(status)
            (since Linux 2.6.10) returns true if the child process was resumed by delivery of SIGCONT.
        @return
        */
        exited = WIFEXITED(status);
        if(exited) {
            return_code = WEXITSTATUS(status);
        }
        signaled = WIFSIGNALED(status);
        if(signaled) {
            signaled_signal = WTERMSIG(status);
            generated_core_dump = WCOREDUMP(status);
        }
        stopped = WIFSTOPPED(status);
        if(stopped) {
            stopped_signal = WSTOPSIG(status);
        }
        continued = WIFCONTINUED(status);

        running = !(signaled || exited);
    }
};

std::ostream& operator<<(std::ostream& out, proc_status& proc) {
    out << "pid: " << proc.pid << "\n";
    out << "running: " << proc.running << "\n";
    out << "status: " << proc.wstatus << "\n";
    if(proc.running) {
        out << "stdin_fd: " << proc.stdin_fd << "\n";
    }
    if(proc.exited) {
        out << "exited: " << proc.exited << "\n"
          << "return_code: " << proc.return_code << "\n";
    }
    if(proc.signaled) {
        out << "signaled: " << proc.signaled << "\n"
            << "signaled_signal: " << proc.signaled_signal << " (" << strsignal(proc.signaled_signal) << ")" << "\n"
            << "generated_core_dump: " << proc.generated_core_dump << "\n";
    }
    if(proc.stopped) {
        out << "stopped: " << proc.stopped << "\n"
            << "stopped_signal: " << proc.stopped_signal << " (" << strsignal(proc.stopped_signal) << ")" << "\n";
    }
    if(proc.continued) {
        out << "continued: " << proc.continued;
    }
    return out;
}

using fd_callback = std::function<void(char * buffer, std::size_t size)>;
using fd_state_callback = std::function<void(proc_status::state, proc_status&)>;

int pipe_signal[2] = {-1};

void signal_handler(int sig) {
    ::write(pipe_signal[1], (void *)&sig, sizeof(sig));
}

template<int BUFFER_SIZE = 1024>
void run_process(const std::string& spawn_process_arg, const fd_state_callback& on_state_cb, const fd_callback& stdout_cb, const fd_callback& stderr_cb) {

    assert(pipe2(pipe_signal, O_CLOEXEC | O_NONBLOCK) >= 0);

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    assert(::sigaction(SIGCHLD, &sa, nullptr) != -1);

    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGCHLD);

    int pipe_stdin[2] = {-1};
    int pipe_stdout[2] = {-1};
    int pipe_stderr[2] = {-1};

    assert(pipe2(pipe_stdin, O_NONBLOCK) >= 0);
    assert(pipe2(pipe_stdout, O_NONBLOCK) >= 0);
    assert(pipe2(pipe_stderr, O_NONBLOCK) >= 0);

    auto args = util::str2arg(spawn_process_arg);
    assert(!args.empty());
    auto args_c = arg2argc(args);

    sigprocmask(SIG_BLOCK, &signal_set, nullptr);
    auto proc_pid = fork();
    assert(proc_pid != -1);
    if(proc_pid == 0) {
        //child
        assert(dup2(pipe_stdin[0], STDIN_FILENO) == STDIN_FILENO);
        assert(dup2(pipe_stdout[1], STDOUT_FILENO) == STDOUT_FILENO);
        assert(dup2(pipe_stderr[1], STDERR_FILENO) == STDERR_FILENO);

        close_pipe(pipe_stdin);
        close_pipe(pipe_stdout);
        close_pipe(pipe_stderr);

        assert(args_c.size() >= 2); // name + nullptr
        execvp(args_c[0], args_c.data());
        //only when file in args_c[0] not found, should not happen
        _Exit(1);
    }
    sigprocmask(SIG_UNBLOCK, &signal_set, nullptr);

    int pipe_close_watch[2] = {-1};
    assert(pipe2(pipe_close_watch, O_NONBLOCK | O_CLOEXEC) >= 0);

    proc_status proc{};

    proc.pid = proc_pid;
    proc.stdin_fd = pipe_stdin[1];
    proc.running = true;

    proc.user_stop_fd = pipe_close_watch[1];

    on_state_cb(proc_status::state::STARTED, proc);

    ssize_t rc;
    fd_set read_fd{};

    std::vector<int> watch_fds{
        pipe_close_watch[0],
        pipe_stdout[0],
        pipe_stderr[0],
        pipe_signal[0]
    };

    auto max_fd = *std::max_element(watch_fds.begin(), watch_fds.end());

    std::array<char, BUFFER_SIZE> read_buffer{};

    bool proc_killed = false;
    bool user_stopped = false;
    int status = 0;

    while(true) {

        bool failed = false;
        int save_errno = errno;
        do {
            FD_ZERO(&read_fd);
            FD_SET(pipe_close_watch[0], &read_fd);
            FD_SET(pipe_stdout[0], &read_fd);
            FD_SET(pipe_stderr[0], &read_fd);
            FD_SET(pipe_signal[0], &read_fd);

            int select_ret = 0;
            errno = 0;
            if ((select_ret = select(max_fd + 1, &read_fd, nullptr, nullptr, nullptr)) < 0) {
                if(errno != EINTR) {
                    std::cout << "select failed ret: " << select_ret << " " << strerror(errno) << " (" << errno << ")" << std::endl;
                    failed = true;
                    break;
                }
            }
        } while(errno == EINTR);
        errno = save_errno;

        if(failed) {
            std::cout << "select failed" << std::endl;
            break;
        }

        if (FD_ISSET(pipe_stdout[0], &read_fd)) {
            if ((rc = read(pipe_stdout[0], read_buffer.data(), read_buffer.size()-1)) < 0) {
                std::cerr << "[" << pipe_stdout[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stdout_cb(read_buffer.data(), rc);
        }

        if (FD_ISSET(pipe_stderr[0], &read_fd)) {
            if ((rc = read(pipe_stderr[0], read_buffer.data(), read_buffer.size()-1)) < 0) {
                std::cerr << "[" << pipe_stderr[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stderr_cb(read_buffer.data(), rc);
        }

        if (FD_ISSET(pipe_signal[0], &read_fd)) {
            int signal = 0;

            int read_from = 0;
            int want_read = sizeof(signal);
            do {
                if ((rc = read(pipe_signal[0], static_cast<void*>((char*)(&signal)+read_from), want_read)) < 0) {
                    std::cerr << "[" << pipe_signal[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                    break;
                }
                read_from += rc;
                want_read -= rc;
            }while(want_read != 0);
            assert(read_from == 4);
            assert(want_read == 0);

            switch(signal) {
                case SIGCHLD: {
                    sigprocmask(SIG_BLOCK, &signal_set, nullptr);

                    pid_t ret;

                    save_errno = errno;
                    do {
                        status = 0;
                        errno = 0;
                        if((ret = waitpid(proc_pid, &status, WNOHANG)) <= 0) {
                            std::cerr << "waitpid(" << proc_pid << ", " << status << ", 0) returned " << ret << std::endl;
                        }
                    } while(errno == EINTR);
                    errno = save_errno;

                    proc.update_status(status);
                    on_state_cb(proc_status::state::SIGNALED, proc);

                    if (!proc.running) {
                        proc.stdin_fd = -1;
                        proc_killed = true;
                    }
                    sigprocmask(SIG_UNBLOCK, &signal_set, nullptr);
                    break;
                }
                default: {
                    std::cout << "signal not handled: " << signal << ", " << strsignal(signal) << std::endl;
                }
            }
            if(proc_killed) {
                break;
            }
        }

        if (FD_ISSET(pipe_close_watch[0], &read_fd)) {
            user_stopped = true;

            do {
                if ((rc = read(pipe_close_watch[0], read_buffer.data(), read_buffer.size()-1)) < 0) {
                    if(errno != EAGAIN) {
                        std::cerr << "[" << pipe_close_watch[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                    }
                    break;
                }
                if (rc == 0) continue;
            }while(true);

            break;
        }
    }

    //reset SIGCHLD signal handler
    signal(SIGCHLD, SIG_DFL);

    if(user_stopped) {
        proc.user_stop_fd = -1;
        on_state_cb(proc_status::state::USER_STOPPED, proc);
    } else {
        // reading rest of the pipe_stdout buffer
        do {
            if ((rc = read(pipe_stdout[0], read_buffer.data(), read_buffer.size()-1)) < 0) {
                if(errno != EAGAIN) {
                    std::cerr << "[" << pipe_stdout[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                }
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stdout_cb(read_buffer.data(), rc);
        }while(true);

        // reading rest of the pipe_stderr buffer
        do {
            if ((rc = read(pipe_stderr[0], read_buffer.data(), read_buffer.size()-1)) < 0) {
                if(errno != EAGAIN) {
                    std::cerr << "[" << pipe_stderr[0] << "] could not read from fd : " << strerror(errno) << " (" << errno << ")" << std::endl;
                }
                break;
            }
            if (rc == 0) continue;
            read_buffer[rc] = 0;
            stderr_cb(read_buffer.data(), rc);
        }while(true);

        on_state_cb(proc_status::state::STOPPED, proc);
    }

    close_pipe(pipe_close_watch);
    close_pipe(pipe_signal);
    close_pipe(pipe_stdout);
    close_pipe(pipe_stderr);
    close_pipe(pipe_stdin);
}

int main() {
    std::string out;
    std::string err;

    std::ostringstream stdout_oss;
    std::ostringstream stderr_oss;
    //data is in chunks that needs to be merged
    fd_callback stdout_cb = [&](char* data, std::size_t len){
        std::cout << "read stdout(size=" << len << "): \n" << data << std::endl;
        stdout_oss << data;
    };
    fd_callback stderr_cb = [&](char* data, std::size_t len){
        std::cout << "read stderr(size=" << len << "): \n" << data << std::endl;
        stderr_oss << data;
    };
    fd_state_callback state_cb = [&](proc_status::state state, proc_status& proc) {
        std::cout << "process " << proc_status::state2str(state) << ": \n" << proc << std::endl;
        std::cout << std::endl;

        if(state == proc_status::state::STARTED) {
            for(int i = 0; i != 10; ++i) {
                std::ostringstream oss;
                oss << "line " << i << "\n";
                auto str = oss.str();
                //write line to input
                write(proc.stdin_fd, str.data(), str.size());
            }

            //stop program
            std::string str = "exit\n";
            write(proc.stdin_fd, str.data(), str.size());

            //stop with signal
            //kill(proc.pid, SIGTERM);
            //kill(proc.pid, SIGKILL);
        }
    };
    run_process<1024>("./simple_echo", state_cb, stdout_cb, stderr_cb);

    out = stdout_oss.str();
    err = stderr_oss.str();

    std::cout << "output(size=" << out.size() << "): \n" << out << std::endl;
    std::cout << "error(size=" << err.size() << "): \n" << err << std::endl;
    return 0;
}
