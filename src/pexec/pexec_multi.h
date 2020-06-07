//
// Created by Michal Němec on 07/06/2020.
//

#ifndef PEXEC_PEXEC_MULTI_H
#define PEXEC_PEXEC_MULTI_H

#include <pexec/signal/sigchld_handler.h>
#include "event/select_event.h"
#include "pexec.h"
#include "exec.h"
#include "queue_buffer.h"

namespace pexec {

using status_cb = std::function<void(const pexec_status&)>;

enum class job_type {
    STOP, SPAWN
};

enum class stop_flag {
    STOP_WAIT, STOP_USER, STOP_KILL
};

struct pexec_job {
    job_type jtype;
    explicit pexec_job(job_type t);
};

struct pexec_stop : public pexec_job {
    stop_flag stop_flag = stop_flag::STOP_WAIT;
    int killnum = -1;
    pexec_stop();
};

class pexec_multi;

class pexec_multi_handle : public pexec_job {
    std::ostringstream stdout_oss_;
    std::ostringstream stderr_oss_;
    pexec_status ret_{};
    pexec<1024> proc_;
    status_cb on_stop_cb_;
    std::function<void()> on_proc_stopped_cb_;
    pexec_fds fds_{};

    void on_proc_stopped(std::function<void()> cb);
    void exec();
    pid_t pid() const noexcept;

public:
    explicit pexec_multi_handle(const std::string &args);
    void on_stop(status_cb cb);

    friend pexec_multi;

};

class pexec_multi {
    select_event loop;
    queue_buffer<std::shared_ptr<pexec_job>> buffer;
    std::unordered_map<pid_t, std::shared_ptr<pexec_multi_handle>> active_procs_;
    error err_ = error::NO_ERROR;
    bool valid_ = false;
    int killnum_ = 0;
    stop_flag stop_flag_ = stop_flag::STOP_WAIT;
    bool stopping_ = false;
    error_status_cb error_cb_;

    void process_error(error err);
    void cleanup();
    void send_job(const std::shared_ptr<pexec_job>& ptr);
    void send_job_nullptr_stop();
    event_return job_stop(const std::shared_ptr<pexec_stop>& stop);
    event_return job_nullptr_stop();
    event_return job_spawn_proc(const std::shared_ptr<pexec_multi_handle>& proc);

public:
    void stop(stop_flag sf, int killnum = -1);
    void on_error(error_status_cb err);
    void run();
    void exec(const std::string& args, const status_cb& cb = {});

};

}

#endif //PEXEC_PEXEC_MULTI_H
