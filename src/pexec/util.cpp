//
// Created by Michal Němec on 06/06/2020.
//



#include "util.h"

namespace pexec {

int
fd_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int
fd_set_cloexec(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_CLOEXEC;
    return fcntl(fd, F_SETFL, flags);
}

int
fd_unset_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

#if defined __APPLE__
int
pipe2(int* p, int flags)
{
    int ret = 0;
    if((ret = pipe(p)) < 0) return -1;
    if(flags == 0) return ret;

    if((flags & O_NONBLOCK) == O_NONBLOCK) {
        if(fd_set_nonblock(p[0]) < 0) {
            close_pipe(p);
            return -1;
        }
        if(fd_set_nonblock(p[1]) < 0) {
            close_pipe(p);
            return -1;
        }
    }
    if((flags & O_CLOEXEC) == O_CLOEXEC) {
        if(fd_set_cloexec(p[0]) < 0) {
            close_pipe(p);
            return -1;
        }
        if(fd_set_cloexec(p[1]) < 0) {
            close_pipe(p);
            return -1;
        }
    }
    return ret;
}
#endif

void
close_fd(int* fd)
{
    if(*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

void
close_pipe(int* pipe)
{
    close_fd(&pipe[0]);
    close_fd(&pipe[1]);
}

std::vector<char *>
arg2argc(const std::vector<std::string>& args)
{
    std::vector<char*> args_c;
    args_c.reserve(args.size()+1);
    for(auto& arg : args) {
        args_c.emplace_back((char*)arg.c_str());
    }
    args_c.emplace_back(nullptr);
    return args_c;
}

int pipe_signal[2] = {-1, -1};

void
signal_handler(int sig)
{
    ::write(pipe_signal[1], (void *)&sig, sizeof(sig));
}

}