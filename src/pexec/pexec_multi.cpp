//
// Created by Michal Němec on 07/06/2020.
//

#include "pexec_multi.h"

using namespace pexec;

void
pexec_multi_handle::on_proc_stopped(std::function<void()> cb)
{
    on_proc_stopped_cb_ = std::move(cb);
}

void pexec_multi_handle::on_stop(stat_cb cb)
{
    on_stop_cb_ = std::move(cb);
}

void
pexec_multi_handle::exec()
{
    proc_.exec(ret_.args);
    // obtaind all file descriptors that we must want on the event loop
    fds_ = proc_.get_fds();
}

pid_t
pexec_multi_handle::pid() const noexcept
{
    return ret_.proc.pid;
}

pexec_multi_handle::pexec_multi_handle(const std::string &args)
: pexec_job(job_type::SPAWN)
{
    ret_.args = args;

    // we will handle all file descriptors in separate event loop
    proc_.set_type(type::NONBLOCKING);

    // register all callbacks
    proc_.set_error_cb([&](error err){
        ret_.err.emplace_back(err, errno);
    });
    proc_.set_stdout_cb([&](const char* data, std::size_t len){
        stdout_oss_ << data;
    });
    proc_.set_stderr_cb([&](const char* data, std::size_t len){
        stderr_oss_ << data;
    });
    proc_.set_state_cb([&](proc_status::state state, proc_status& stat) {
        ret_.proc = stat;
        ret_.state = state;
        if(state == proc_status::state::STOPPED || state == proc_status::state::USER_STOPPED || state == proc_status::state::FAIL_STOPPED) {
            ret_.proc_out = stdout_oss_.str();
            ret_.proc_err = stderr_oss_.str();
            // internal callback for handling event loop operations
            // this will call ::on_proc_stopped registered callback that will detach file descriptors from event loop
            if(on_proc_stopped_cb_) {
                on_proc_stopped_cb_();
            }

            // user callback ::on_stop
            if(on_stop_cb_) {
                on_stop_cb_(ret_);
            }
        }
    });
}