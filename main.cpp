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

#include "argument_parser.h"

using fd_callback = std::function<void(char * buffer, std::size_t size)>;

int pipe_signal[2] = {-1};

void signal_handler(int sig) {
    ::write(pipe_signal[1], (void *)&sig, sizeof(sig));
}

template<int BUFFER_SIZE = 1024>
void run_process(const std::string& spawn_process_arg, const fd_callback& stdout_cb, const fd_callback& stderr_cb) {

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

    sigprocmask(SIG_BLOCK, &signal_set, nullptr);
    auto proc_pid = fork();
    assert(proc_pid != -1);
    if(proc_pid == 0) {
        //child
        auto args = util::str2arg(spawn_process_arg);
        assert(!args.empty());

        std::vector<char*> args_c;
        args_c.reserve(args.size()+1);
        for(auto& arg : args) {
            args_c.emplace_back((char*)arg.c_str());
        }
        args_c.emplace_back(nullptr);

        close(pipe_stdin[1]);
        assert(dup2(pipe_stdin[0], STDIN_FILENO) == STDIN_FILENO);
        close(pipe_stdin[0]);

        close(pipe_stdout[0]);
        assert(dup2(pipe_stdout[1], STDOUT_FILENO) == STDOUT_FILENO);
        close(pipe_stdout[1]);

        close(pipe_stderr[0]);
        assert(dup2(pipe_stderr[1], STDERR_FILENO) == STDERR_FILENO);
        close(pipe_stderr[1]);

        assert(args_c.size() >= 2); // name + nullptr
        execvp(args_c[0], args_c.data());

        //only when file in args_c[0] not found, should not happen
        _Exit(1);
    }
    sigprocmask(SIG_UNBLOCK, &signal_set, nullptr);

    int close_pipe_read[2] = {-1};
    assert(pipe2(close_pipe_read, O_CLOEXEC) >= 0);

    ssize_t rc;
    fd_set read_fd{};

    std::vector<int> watch_fds{
        pipe_stdout[0],
        pipe_stderr[0],
        pipe_signal[0]
    };

    auto max_fd = *std::max_element(watch_fds.begin(), watch_fds.end());

    std::array<char, BUFFER_SIZE> read_buffer{};

    bool proc_killed = false;

    bool proc_exited = false;
    int proc_return_code = 0;

    bool proc_signalled = false;
    int proc_signal_term = 0;

    while(true) {

        bool failed = false;
        int save_errno = errno;
        do {
            FD_ZERO(&read_fd);
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
            std::cout << "signal pipe" << std::endl;
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
                    int status;
                    auto save_errno = errno;
                    errno = 0;

                    do {
                        if((ret = waitpid(proc_pid, &status, WNOHANG)) <= 0) {
                            std::cerr << "waitpid(" << proc_pid << ", " << status << ", 0) returned " << ret << std::endl;
                        }
                    } while(errno == EINTR);
                    errno = save_errno;

                    bool exited = WIFEXITED(status);
                    bool signalled = WIFSIGNALED(status);
                    if (exited || signalled) {
                        if(exited) {
                            proc_exited = true;
                            proc_return_code = WEXITSTATUS(status);
                        }
                        if(signalled) {
                            proc_signalled = true;
                            proc_signal_term = WTERMSIG(status);
                        }
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
                //while loop
                break;
            }
        }
    }

    //reset SIGCHLD signal handler
    signal(SIGCHLD, SIG_DFL);

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

    close(close_pipe_read[0]);
    close(close_pipe_read[1]);

    close(pipe_signal[0]);
    close(pipe_signal[1]);

    close(pipe_stdout[0]);
    close(pipe_stdout[1]);

    close(pipe_stdin[0]);
    close(pipe_stdin[1]);

    close(pipe_stderr[0]);
    close(pipe_stderr[1]);

    std::cout << "process " << proc_pid << " stopped" << std::endl;
    if(proc_exited) {
        std::cout << "return code: " << proc_return_code << std::endl;
    }
    if(proc_signalled) {
        std::cout << "by signal: " << strsignal(proc_signal_term) << " (" << proc_signal_term << ")" << std::endl;
    }
}

void watch_process(const std::string& args, std::string& out, std::string& err) {
    std::ostringstream stdout_oss;
    std::ostringstream stderr_oss;
    //data is in chunks that needs to be merged
    fd_callback stdout_cb = [&](char* data, std::size_t len){
        stdout_oss << data;
    };

    fd_callback stderr_cb = [&](char* data, std::size_t len){
        stderr_oss << data;
    };

    run_process<8048>(args, stdout_cb, stderr_cb);

    out = stdout_oss.str();
    err = stderr_oss.str();
}


int main() {
    std::string out;
    std::string err;

    watch_process("ls -l /Library", out, err);

    std::cout << "output(size=" << out.size() << "): \n" << out << std::endl;
    std::cout << "error(size=" << err.size() << "): \n" << err << std::endl;
    return 0;
}
