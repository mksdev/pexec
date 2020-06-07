//
// Created by Michal Němec on 07/06/2020.
//

#ifndef PEXEC_SIGCHLD_HANDLER_H
#define PEXEC_SIGCHLD_HANDLER_H

#include <csignal>
#include "../error.h"
#include "../event/select_event.h"

namespace pexec {

extern struct sigaction sigchld_sa_;
extern struct sigaction sigchld_sa_prev_;
extern int sigchld_pipe_signal[2];
extern bool sigchld_active;

void sigchld_signal_handler(int sig);
bool sigchld_set_signal_handler();
bool sigchld_reset_signal_handler();
int sigchld_get_signal_fd();

using sigchld_cb = std::function<void(pid_t, int status)>;

class sigchld_handler {
    sigchld_cb cb_;
    sigset_t signal_set_{};
    error err_ = error::NO_ERROR;
    error_status_cb error_cb_;
    void handle_sigchld();
    void prepare_signal_set();
    void block_sigchld();
    void unblock_sigchld();
    void process_error(error err);

public:
    sigchld_handler();
    sigchld_handler(const sigchld_handler&) = delete;
    sigchld_handler& operator=(const sigchld_handler&) = delete;
    sigchld_handler(sigchld_handler&&) = delete;
    sigchld_handler& operator=(sigchld_handler&&) = delete;
    ~sigchld_handler();

    event_return read_signal();
    void on_error(error_status_cb err);
    void on_signal(sigchld_cb cb);
    int get_read_fd();
    bool valid() const noexcept;
    error last_error() const noexcept;

    operator bool() const;

};

}

#endif //PEXEC_SIGCHLD_HANDLER_H
