//
// Created by Michal Němec on 06/06/2020.
//

#ifndef PEXEC_UTIL_H
#define PEXEC_UTIL_H

#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <functional>

#include "proc_status.h"

namespace pexec {

using fd_callback = std::function<void(const char* buffer, std::size_t size)>;
using fd_state_callback = std::function<void(proc_status::state, proc_status&)>;


int fd_set_nonblock(int fd);
int fd_set_cloexec(int fd);
int fd_unset_nonblock(int fd);

#if defined __APPLE__
int pipe2(int* p, int flags);
#endif

void close_fd(int* fd);
void close_pipe(int* pipe);
std::vector<char *> arg2argc(const std::vector<std::string>& args);

extern int sigchld_blocking_pipe_signal[2];
void sigchld_blocking_signal_handler(int sig);

}

#endif //PEXEC_UTIL_H
