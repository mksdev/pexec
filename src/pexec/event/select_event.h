//
// Created by Michal Němec on 07/06/2020.
//

#ifndef PEXEC_SELECT_EVENT_H
#define PEXEC_SELECT_EVENT_H

#include <cstdio>
#include <unistd.h>

#include <vector>
#include <functional>
#include <unordered_map>
#include <cerrno>
#include <iostream>

#include "../util.h"

namespace pexec {


enum class event_return {
    STOP_LOOP, SKIP_OTHER_FD, NOTHING
};

using read_event_cb = std::function<event_return(int fd)>;

class select_event {
    int control_pipe[2] = {-1, -1};
    std::function<event_return()> on_interrupt_;
    std::function<void()> on_select_error_;
    bool actual_iterator_deleted_ = false;
    std::unordered_map<int, read_event_cb>::iterator actual_iterator_cbs_;
    std::unordered_map<int, read_event_cb> cbs_;
    bool recompute_max_fds = true;
    int max_fd_ = -1;

    int get_max_fd();

public:
    select_event();
    select_event(const select_event&) = delete;
    select_event& operator=(const select_event&) = delete;
    select_event(select_event&&) = delete;
    select_event& operator=(select_event&&) = delete;

    ~select_event();
    void on_interrupt(std::function<event_return()> cb);
    void on_error(std::function<void()> cb);
    void interrupt() const;
    int interrupt_write_fd() const noexcept;
    void add_read_event(int fd, read_event_cb cb);
    void remove_read_event(int fd);
    void loop();

};

}


#endif //PEXEC_SELECT_EVENT_H
