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

using stat_cb = std::function<void(const pexec_status&)>;

enum class job_type {
    STOP, SPAWN
};

enum class stop_flag {
    STOP_WAIT, STOP_USER, STOP_KILL
};

struct pexec_job {
    job_type jtype;
    explicit pexec_job(job_type t) : jtype(t) {}
};

struct pexec_stop : public pexec_job {

    stop_flag stop_flag = stop_flag::STOP_WAIT;
    int killnum = -1;

    pexec_stop() : pexec_job(job_type::STOP) {

    }
};

class pexec_multi;

class pexec_multi_handle : public pexec_job {

    std::ostringstream stdout_oss_;
    std::ostringstream stderr_oss_;

    pexec_status ret_{};
    pexec<> proc_;
    stat_cb on_stop_cb_;
    std::function<void()> on_proc_stopped_cb_;
    pexec_fds fds_{};

    void on_proc_stopped(std::function<void()> cb);
    void exec();
    pid_t pid() const noexcept;

public:
    explicit pexec_multi_handle(const std::string &args);

    void on_stop(stat_cb cb);

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

    void process_error(error err) {
        if(error_cb_) {
            error_cb_(err);
        }
    }

    void cleanup() {
        stopping_ = true;
        switch (stop_flag_) {
            case stop_flag::STOP_USER: {
                for(auto&& p : active_procs_) {
                    auto proc = p.second;
                    proc->ret_.proc.user_stop();
                }
                break;
            }
            case stop_flag::STOP_KILL: {
                for(auto&& p : active_procs_) {
                    ::kill(p.first, killnum_);
                }
                break;
            }
            case stop_flag::STOP_WAIT: {
                break;
            }
        }
    }

    void send_job(const std::shared_ptr<pexec_job>& ptr) {
        buffer.add(ptr);
        loop.interrupt();
    }

    void send_job_nullptr_stop() {
        send_job(nullptr);
    }

    event_return job_stop(const std::shared_ptr<pexec_stop>& stop) {
        if(stopping_) {
            return event_return::NOTHING;
        }
        stop_flag_ = stop->stop_flag;
        killnum_ = stop->killnum;
        return job_nullptr_stop();
    }

    event_return job_nullptr_stop() {
        if(!stopping_) {
            cleanup();
            return event_return::NOTHING;
        }
        if(active_procs_.empty()) {
            return event_return::STOP_LOOP;
        }
        return event_return::NOTHING;
    }

    event_return job_spawn_proc(const std::shared_ptr<pexec_multi_handle>& proc) {
        if(stopping_) {
            // do not spawn any new processes when we are in stop state
            return event_return::NOTHING;
        }

        // execute ::fork and duplicate file descriptors
        proc->exec();

        // get pid information
        auto pid = proc->pid();
        assert(pid > 0);

        // save for sigchld mapping
        active_procs_[pid] = proc;

        // register on stop callback
        proc->on_proc_stopped([=]() {
            // remove registered file descriptors
            loop.remove_read_event(proc->fds_.watch_close_write_fd);
            loop.remove_read_event(proc->fds_.stdout_read_fd);
            loop.remove_read_event(proc->fds_.stderr_read_fd);

            // delete from sigchld mapping;
            auto it = active_procs_.find(pid);
            if(it != active_procs_.end()) {
                active_procs_.erase(it);
            }

            if(stopping_) {
                if(active_procs_.empty()) {
                    send_job_nullptr_stop();
                }
            }
        });

        // register process duplicated file descriptors
        // manual stopping of the process watching
        loop.add_read_event(proc->fds_.watch_close_write_fd, [=](int fd){
            return proc->proc_.read_close();
        });
        // reading duplicated stdout output
        loop.add_read_event(proc->fds_.stdout_read_fd, [=](int fd){
            return proc->proc_.read_stdout();
        });
        // reading duplicated stderr output
        loop.add_read_event(proc->fds_.stderr_read_fd, [=](int fd){
            return proc->proc_.read_stderr();
        });
        return event_return::NOTHING;
    }

public:
    void exec(const std::string& args, const stat_cb& cb = {})
    {
        auto proc = std::make_shared<pexec_multi_handle>(args);
        proc->on_stop(cb);
        send_job(proc);
    }

    void stop(stop_flag sf, int killnum = -1)
    {
        auto stop_job = std::make_shared<pexec_stop>();
        stop_job->stop_flag = sf;
        stop_job->killnum = killnum;
        send_job(stop_job);
    }

    void on_error(error_status_cb err)
    {
        error_cb_ = std::move(err);
    }

    void run() {
        // prepare separate signal handler
        sigchld_handler sigchld{loop};
        if(!sigchld) {
            process_error(sigchld.last_error());
            return;
        }
        sigchld.on_error([&](error err){
            process_error(err);
        });

        // callback for ::waitpid results
        sigchld.on_signal([&](pid_t pid, int status){
            auto it = active_procs_.find(pid);
            if(it != active_procs_.end()) {
                it->second->proc_.update_status(status);
            }
        });

        // register signal handler pipe for processing SIGCHLD signals
        loop.add_read_event(sigchld.get_read_fd(), [&](int fd){
            return sigchld.read_signal();
        });

        loop.on_error([&](){
            process_error(error::SELECT_ERROR);
        });

        // called when loop.interrupt(); is executed
        // we use producer/consumer buffer to pass jobs into our event loop
        // job can be for spawning new process of stopping the event-loop
        loop.on_interrupt([&](){
            auto job = buffer.remove();
            if(job == nullptr) {
                return job_nullptr_stop();
            }
            switch (job->jtype) {
                case job_type::STOP: return job_stop(std::static_pointer_cast<pexec_stop>(job));
                case job_type::SPAWN: return job_spawn_proc(std::static_pointer_cast<pexec_multi_handle>(job));
            }
            return event_return::NOTHING;
        });

        // run simple ::select based event loop
        loop.loop();
    }
};

}

#endif //PEXEC_PEXEC_MULTI_H
