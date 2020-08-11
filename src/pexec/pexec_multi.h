//
// Created by Michal Němec on 07/06/2020.
//

#ifndef PEXEC_PEXEC_MULTI_H
#define PEXEC_PEXEC_MULTI_H

#include <cassert>

#include "signal/sigchld_handler.h"
#include "event/select_event.h"
#include "pexec_single.h"
#include "pexec_status.h"
#include "queue_buffer.h"

namespace pexec {

using status_cb = std::function<void(const pexec_status&)>;

enum class job_type {
    // sets up stopping criterion
    STOP,
    // passes process spawn information
    SPAWN
};

enum class loop_type {
    DEFAULT,
    EXTERNAL
};

enum class fd_action {
    ADD_EVENT,
    REMOVE_EVENT
};

enum class fd_what {
    READ, WRITE
};

enum class stop_flag : int {
    // wait until all active processes has exited
    STOP_WAIT,
    // detach process from event loop, close all suplicated file descriptors
    // process in not killed, any ::waitpid calls must be performed by user
    STOP_USER,
    // call ::kill() on active processes
    STOP_KILL
};

struct pexec_job {
    job_type job_type_;
    explicit pexec_job(job_type t);
};

struct pexec_stop : public pexec_job {
    stop_flag stop = stop_flag::STOP_WAIT;
    int signum = -1; // SIGKILL(9), SIGTERM(15), etc..
    pexec_stop();
};

class pexec_multi;

class pexec_multi_handle : public pexec_job {

    pexec<1024> proc_;
    std::function<void()> on_proc_stopped_cb_;

    // file descriptors for event loop
    pexec_fds fds_{};

    // custom callbacks for pexec_multi::exec(.. proc_cb);
    fd_callback stdout_cb_;
    fd_callback stderr_cb_;
    fd_state_callback state_cb_;
    error_status_cb error_cb_;

    // return callback for pexec_multi::exec(.. status_cb);
    pexec_status ret_{};
    std::ostringstream stdout_oss_;
    std::ostringstream stderr_oss_;
    status_cb on_stop_cb_;

    void on_stop(status_cb cb);
    void on_proc_stopped(std::function<void()> cb);
    void exec();
    pid_t pid() const noexcept;

public:
    ~pexec_multi_handle() {
        std::cout << "destruct" << std::endl;
    }
    explicit pexec_multi_handle(const std::string &args);
    void set_stdout_cb(fd_callback cb);
    void set_stderr_cb(fd_callback cb);
    void set_state_cb(fd_state_callback cb);
    void set_error_cb(error_status_cb cb);

    friend pexec_multi;

};

using proc_cb = std::function<void(pexec_multi_handle&)>;

class pexec_multi {
    int control_pipe[2] = {-1, -1};
    loop_type type = loop_type::DEFAULT;

    // prepare separate signal handler
    std::unique_ptr<sigchld_handler> sigchld;
    std::unique_ptr<select_event> loop;

    // thread-safe job buffer
    queue_buffer<std::shared_ptr<pexec_job>> buffer;

    // keep track of executed processes
    std::unordered_map<pid_t, std::shared_ptr<pexec_multi_handle>> active_procs_;

    // error handling
    error err_ = error::NO_ERROR;
    error_status_cb error_cb_;

    std::unordered_map<int, std::function<void(int)>> registered_fd_;
    std::function<void(int, fd_action, fd_what)> register_function_cb_;

    // stopping criterion
    stop_flag stop_flag_ = stop_flag::STOP_WAIT;
    int stop_signum_ = 0;
    bool stopping_ = false;

    void process_error(error err);
    void cleanup();
    void send_job(const std::shared_ptr<pexec_job>& ptr);
    void send_job_nullptr_stop();
    event_return job_stop(const std::shared_ptr<pexec_stop>& stop);
    event_return job_nullptr_stop();
    event_return job_spawn_proc(const std::shared_ptr<pexec_multi_handle>& proc);
    void add_read_event(int fd, const std::function<void(int)>& cb);
    void remove_read_event(int fd);
    void interrupt() {
        char c = '\0';
        ::write(control_pipe[1], &c, 1);
    }
    void handle_stop();

public:
    pexec_multi() {
        auto ret = pipe2(control_pipe, O_CLOEXEC | O_NONBLOCK);
        assert(ret >= 0);
    }
    ~pexec_multi() {
        close_pipe(control_pipe);
    }

    void register_event(std::function<void(int, fd_action, fd_what)> cb);
    void do_action(int fd);
    event_return dispatch_job();
    void on_error(error_status_cb err);
    error last_error() const noexcept;
    void run();
    void exec(const std::string& args, const status_cb& cb = {});
    void exec(const std::string& args, const proc_cb& cb = {});
    void stop(stop_flag sf, int killnum = -1);
    void set_type(loop_type type);
};

}

#endif //PEXEC_PEXEC_MULTI_H
