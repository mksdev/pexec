//
// Created by Michal Němec on 07/06/2020.
//

#include "pexec_multi.h"
#include <cassert>

using namespace pexec;

pexec_job::pexec_job(job_type t)
: job_type_(t)
{

}

pexec_stop::pexec_stop()
: pexec_job(job_type::STOP)
{

}

void
pexec_multi_handle::on_proc_stopped(std::function<void()> cb)
{
    on_proc_stopped_cb_ = std::move(cb);
}

void pexec_multi_handle::on_stop(status_cb cb)
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

void
pexec_multi_handle::set_stdout_cb(fd_callback cb)
{
    stdout_cb_ = std::move(cb);
}

void
pexec_multi_handle::set_stderr_cb(fd_callback cb)
{
    stderr_cb_ = std::move(cb);
}

void
pexec_multi_handle::set_state_cb(fd_state_callback cb)
{
    state_cb_ = std::move(cb);
}

void
pexec_multi_handle::set_error_cb(error_status_cb cb)
{
    error_cb_ = std::move(cb);
}

pexec_multi_handle::pexec_multi_handle(const std::string &args)
: pexec_job(job_type::SPAWN)
{
    ret_.args = args;

    // we will handle all file descriptors in separate event loop
    proc_.set_type(type::NONBLOCKING);

    // register all callbacks
    proc_.set_error_cb([&](error err){
        if(error_cb_) {
            error_cb_(err);
        } else {
            ret_.err.emplace_back(err, errno);
        }
    });
    proc_.set_stdout_cb([&](const char* data, std::size_t len){
        if(stdout_cb_) {
            stdout_cb_(data, len);
        } else {
            stdout_oss_ << data;
        }
    });
    proc_.set_stderr_cb([&](const char* data, std::size_t len){
        if(stderr_cb_) {
            stderr_cb_(data, len);
        } else {
            stderr_oss_ << data;
        }
    });
    proc_.set_state_cb([&](proc_status::state state, proc_status& stat) {
        if(state_cb_) {
            state_cb_(state, stat);
        }
        ret_.proc = stat;
        ret_.state = state;
        if(state == proc_status::state::STOPPED || state == proc_status::state::USER_STOPPED || state == proc_status::state::FAIL_STOPPED) {
            ret_.proc_out = stdout_oss_.str();
            ret_.proc_err = stderr_oss_.str();

            // user callback ::on_stop
            if(on_stop_cb_) {
                on_stop_cb_(ret_);
            }

            // internal callback for handling event loop operations
            // this will call ::on_proc_stopped registered callback that will detach file descriptors from event loop
            if(on_proc_stopped_cb_) {
                on_proc_stopped_cb_();
            }

        }
    });
}

void
pexec_multi::process_error(error err)
{
    err_ = err;
    if(error_cb_) {
        error_cb_(err);
    }
}

void
pexec_multi::cleanup()
{
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
                ::kill(p.first, stop_signum_);
            }
            break;
        }
        case stop_flag::STOP_WAIT: {
            break;
        }
    }
}

void
pexec_multi::send_job(const std::shared_ptr<pexec_job>& ptr)
{
    buffer.add(ptr);
    interrupt();
}

void
pexec_multi::send_job_nullptr_stop()
{
    send_job(nullptr);
}

event_return
pexec_multi::job_stop(const std::shared_ptr<pexec_stop>& stop)
{
    if(stopping_) {
        return event_return::NOTHING;
    }
    stop_flag_ = stop->stop;
    stop_signum_ = stop->signum;
    return job_nullptr_stop();
}

event_return
pexec_multi::job_nullptr_stop()
{
    if(!stopping_) {
        cleanup();
        return event_return::NOTHING;
    }
    if(active_procs_.empty()) {
        return event_return::STOP_LOOP;
    }
    return event_return::NOTHING;
}

event_return
pexec_multi:: job_spawn_proc(const std::shared_ptr<pexec_multi_handle>& proc)
{
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
    std::weak_ptr<pexec_multi_handle> weak_proc = proc;
    proc->on_proc_stopped([&, pid, weak_proc]() {
        auto p = weak_proc.lock();
        assert(p != nullptr);

        // remove registered file descriptors
        remove_read_event(p->fds_.watch_close_write_fd);
        remove_read_event(p->fds_.stdout_read_fd);
        remove_read_event(p->fds_.stderr_read_fd);

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
    add_read_event(proc->fds_.watch_close_write_fd, [=](int fd){
        return proc->proc_.read_close();
    });
    // reading duplicated stdout output
    add_read_event(proc->fds_.stdout_read_fd, [=](int fd){
        return proc->proc_.read_stdout();
    });
    // reading duplicated stderr output
    add_read_event(proc->fds_.stderr_read_fd, [=](int fd){
        return proc->proc_.read_stderr();
    });
    return event_return::NOTHING;
}

void
pexec_multi::exec(const std::string& args, const status_cb& cb)
{
    auto proc = std::make_shared<pexec_multi_handle>(args);
    if(proc == nullptr) {
        process_error(error::EXEC_ALLOCATION_ERROR);
        return;
    }
    proc->on_stop(cb);
    send_job(proc);
}

void
pexec_multi::exec(const std::string& args, const proc_cb& cb)
{
    auto proc = std::make_shared<pexec_multi_handle>(args);
    if(proc == nullptr) {
        process_error(error::EXEC_ALLOCATION_ERROR);
        return;
    }
    cb(*proc);
    send_job(proc);
}

void
pexec_multi::stop(stop_flag sf, int killnum)
{
    auto stop_job = std::make_shared<pexec_stop>();
    stop_job->stop = sf;
    stop_job->signum = killnum;
    send_job(stop_job);
}

void
pexec_multi::register_event(std::function<void(int, fd_action, fd_what)> cb)
{
    register_function_cb_ = std::move(cb);
}

void
pexec_multi::add_read_event(int fd, const std::function<void(int)>& cb)
{
    registered_fd_[fd] = cb;
    if(register_function_cb_) {
        register_function_cb_(fd, fd_action::ADD_EVENT, fd_what::READ);
    }
}

void
pexec_multi::remove_read_event(int fd)
{
    auto it = registered_fd_.find(fd);
    if(it != registered_fd_.end()) {
        registered_fd_.erase(it);
    }
    if(register_function_cb_) {
        register_function_cb_(fd, fd_action::REMOVE_EVENT, fd_what::READ);
    }
}

void
pexec_multi::do_action(int fd)
{
    registered_fd_[fd](fd);
}


void
pexec_multi::on_error(error_status_cb err)
{
    error_cb_ = std::move(err);
}

error
pexec_multi::last_error() const noexcept
{
    return err_;
}

event_return
pexec_multi::dispatch_job() {
    auto job = buffer.remove();
    if(job == nullptr) {
        return job_nullptr_stop();
    }
    switch (job->job_type_) {
        case job_type::STOP: return job_stop(std::static_pointer_cast<pexec_stop>(job));
        case job_type::SPAWN: return job_spawn_proc(std::static_pointer_cast<pexec_multi_handle>(job));
    }
    return event_return::NOTHING;
}

void
pexec_multi::handle_stop() {
    // remove sigchld ginal handler pipe
    remove_read_event(sigchld->get_read_fd());
    remove_read_event(control_pipe[0]);

    // reset stopping flags to enable re-run
    stop_flag_ = stop_flag::STOP_WAIT;
    stop_signum_ = -1;
    stopping_ = false;

    if(type == loop_type::DEFAULT) {
        loop->interrupt();
    }
}

void
pexec_multi::run()
{
    if(type == loop_type::DEFAULT) {
        loop = std::unique_ptr<select_event>(new select_event());
        register_event([&](int fd, fd_action act, fd_what) {
            switch (act) {
                case fd_action::ADD_EVENT: {
                    loop->add_read_event(fd, [&](int fd_action){
                        do_action(fd_action);
                        return event_return::NOTHING;
                    });
                    break;
                }
                case fd_action::REMOVE_EVENT: {
                    loop->remove_read_event(fd);
                    break;
                }
            }
        });

        loop->on_error([&](){
            process_error(error::SELECT_ERROR);
        });
        // called when loop.interrupt(); is executed
        // we use producer/consumer buffer to pass jobs into our event loop
        // job can be for spawning new process of stopping the event-loop
        loop->on_interrupt([&](){
            return event_return::STOP_LOOP;
        });
    }

    sigchld = std::unique_ptr<sigchld_handler>(new sigchld_handler);
    assert(sigchld);
    if(!sigchld->valid()) {
        process_error(sigchld->last_error());
        return;
    }

    sigchld->on_error([&](error err){
        process_error(err);
    });

    // callback for ::waitpid results
    sigchld->on_signal([&](pid_t pid, int status){
        auto it = active_procs_.find(pid);
        if(it != active_procs_.end()) {
            it->second->proc_.update_status(status);
        }
    });
// register signal handler pipe for processing SIGCHLD signals
    add_read_event(sigchld->get_read_fd(), [&](int fd){
        auto event_ret = sigchld->read_signal();
        if(event_ret == event_return::STOP_LOOP) {
            handle_stop();
        }
    });

    add_read_event(control_pipe[0], [&](int fd){
        char c;
        ssize_t ret;
        do {
            errno = 0;
            ret = ::read(fd, &c, sizeof(c));
        } while (errno == EINTR);
        assert(ret >= 0);

        auto event_ret = dispatch_job();
        if(event_ret == event_return::STOP_LOOP) {
            handle_stop();
        }
    });

    if(type == loop_type::DEFAULT) {
        // run simple ::select based event loop
        loop->loop();
        loop.reset();

        // when using external signal is destructed when run is repeated or in destructor
        sigchld.reset();
    }

}

void
pexec_multi::set_type(loop_type lt)
{
    type = lt;
}