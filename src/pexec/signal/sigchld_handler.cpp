//
// Created by Michal Němec on 07/06/2020.
//

#include <cstdlib>
#include <unistd.h>
#include <csignal>

#include "sigchld_handler.h"
#include "../util.h"

using namespace pexec;

namespace pexec {

struct sigaction sigchld_sa_{};
struct sigaction sigchld_sa_prev_{};
int sigchld_pipe_signal[2] = {-1, -1};
bool sigchld_active = false;

void
sigchld_signal_handler(int sig)
{
    ::write(sigchld_pipe_signal[1], (void *)&sig, sizeof(sig));
}

bool
sigchld_set_signal_handler()
{
    if(pipe2(sigchld_pipe_signal, O_CLOEXEC | O_NONBLOCK) < 0) {
        return false;
    }

    // set signal handler for SIGCHILD
    sigchld_sa_.sa_handler = sigchld_signal_handler;
    sigemptyset(&sigchld_sa_.sa_mask);
    sigchld_sa_.sa_flags = 0;
    {
        // set signal handler, save previous
        auto sigaction_ret = ::sigaction(SIGCHLD, &sigchld_sa_, &sigchld_sa_prev_);
        if(sigaction_ret == -1) {
            close_pipe(sigchld_pipe_signal);
            return false;
        }
    }
    sigchld_active = true;
    return true;
}

bool
sigchld_reset_signal_handler()
{
    if(!sigchld_active) {
        return false;
    }
    //reset SIGCHLD signal handler
    auto sigaction_ret = ::sigaction(SIGCHLD, &sigchld_sa_prev_, nullptr);
    if(sigaction_ret == -1) {
        return false;
    }
    close_pipe(sigchld_pipe_signal);
    sigchld_active = false;
    return true;
}

int
sigchld_get_signal_fd()
{
    return sigchld_pipe_signal[0];
}

}

sigchld_handler::sigchld_handler()
{
    prepare_signal_set();
    if(!sigchld_set_signal_handler()) {
        process_error(error::SIGACTION_SET_ERROR);
        return;
    }
}

sigchld_handler::~sigchld_handler()
{
    if(!sigchld_reset_signal_handler()) {
        process_error(error::SIGACTION_RESET_ERROR);
    }
}

void
sigchld_handler::handle_sigchld()
{
    block_sigchld();
    int save_errno = errno;
    do {
        pid_t pid = 0;
        int status = 0;
        errno = 0;
        // SIGCHLD is not queued in the system so multiple childs can be killed, but only one SIGCHLD is generated
        /*
         * from: https://linux.die.net/man/3/waitpid
         *
         * Return Value
         * If wait() or waitpid() returns because the status of a child process is available,
         * these functions shall return a value equal to the process ID of the child process for which status is
         * reported. If wait() or waitpid() returns due to the delivery of a signal to the calling process, -1 shall be
         * returned and errno set to [EINTR]. If waitpid() was invoked with WNOHANG set in options,
         * it has at least one child process specified by pid for which status is not available,
         * and status is not available for any process specified by pid, 0 is returned. Otherwise,
         * (pid_t)-1 shall be returned, and errno set to indicate the error.
         * The waitpid() function shall fail if:
         * ECHILD
         *      The process specified by pid does not exist or is not a child of the calling process,
         *      or the process group specified by pid does not exist or does not have any member process
         *      that is a child of the calling process.
         * EINTR
         *      The function was interrupted by a signal. The value of the location pointed to by stat_loc is undefined.
         * EINVAL
         *      The options argument is not valid.
         */
        if((pid = ::waitpid(0, &status, WNOHANG)) < 0) {
            if(errno == ECHILD) {
                // no childs to process
                break;
            } else if(errno == EINTR) {
                // some signal has been delivered and interrupted syscall
                continue;
            }
            process_error(error::WAITPID_ERROR);
            break;
        }
        if(pid == 0) {
            // "status is not available for any process specified"
            break;
        }
        if(cb_) {
            cb_(pid, status);
        }
    } while(true);
    errno = save_errno;
    unblock_sigchld();
}

void
sigchld_handler::prepare_signal_set()
{
    sigemptyset(&signal_set_);
    sigaddset(&signal_set_, SIGCHLD);
}

void
sigchld_handler::block_sigchld()
{
    if(::sigprocmask(SIG_BLOCK, &signal_set_, nullptr) == -1) {
        process_error(error::SIG_BLOCK_ERROR);
    }
}

void
sigchld_handler::unblock_sigchld()
{
    if(::sigprocmask(SIG_UNBLOCK, &signal_set_, nullptr) == -1) {
        process_error(error::SIG_UNBLOCK_ERROR);
    }
}

void
sigchld_handler::process_error(error err)
{
    err_ = err;
    if(error_cb_) {
        error_cb_(err);
    }
}

event_return
sigchld_handler::read_signal()
{
    ssize_t rc;
    int fd  = sigchld_pipe_signal[0];
    int signal = 0;
    int read_from = 0;
    int want_read = sizeof(signal);
    do {
        if ((rc = read(fd, static_cast<void*>((char*)(&signal) + read_from), want_read)) < 0) {
            if(errno == EINTR) {
                continue;
            }
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
    return event_return::NOTHING;
}

void
sigchld_handler::on_error(error_status_cb err)
{
    error_cb_ = std::move(err);
}

void
sigchld_handler::on_signal(sigchld_cb cb)
{
    cb_ = std::move(cb);
}

int
sigchld_handler::get_read_fd()
{
    return sigchld_get_signal_fd();
}

bool
sigchld_handler::valid() const noexcept
{
    return err_ == error::NO_ERROR;
}

error
sigchld_handler::last_error() const noexcept
{
    return err_;
}

sigchld_handler::operator bool() const
{
    return valid();
}