//
// Created by Michal Němec on 07/06/2020.
//

#include "select_event.h"

namespace pexec {

int
select_event::get_max_fd()
{
    if(recompute_max_fds) {
        max_fd_ = -1;
        for(auto&& p : cbs_) {
            if(p.first > max_fd_) {
                max_fd_ = p.first;
            }
        }
    }
    return max_fd_;
}

select_event::select_event()
{
    auto ret = pipe2(control_pipe, O_CLOEXEC | O_NONBLOCK);
    assert(ret >= 0);
    add_read_event(control_pipe[0], [&](int fd){
        char c;
        ssize_t ret;
        do {
            errno = 0;
            ret = ::read(fd, &c, sizeof(c));
        } while (errno == EINTR);
        assert(ret >= 0);

        if(on_interrupt_) {
            return on_interrupt_();
        }
        return event_return::NOTHING;
    });
}

select_event::~select_event() {
    close_pipe(control_pipe);
}

void
select_event::on_interrupt(std::function<event_return()> cb)
{
    on_interrupt_ = std::move(cb);
}

void
select_event::on_error(std::function<void()> cb)
{
    on_select_error_ = std::move(cb);
}

void
select_event::interrupt() const
{
    char c = '\0';
    ::write(interrupt_write_fd(), &c, sizeof(c));
}

int
select_event::interrupt_write_fd() const noexcept
{
    return control_pipe[1];
}

void
select_event::add_read_event(int fd, read_event_cb cb)
{
    assert(cbs_.find(fd) == cbs_.end());
    cbs_[fd] = std::move(cb);
}

void
select_event::remove_read_event(int fd)
{
    auto it = cbs_.find(fd);
    if(it != cbs_.end()) {
        recompute_max_fds = true;
        if(actual_iterator_cbs_ == it) {
            actual_iterator_deleted_ = true;
            actual_iterator_cbs_ = cbs_.erase(it);
        } else {
            cbs_.erase(it);
        }
    } else {
        // file descriptor not watched
        assert(0);
    }
}

void
select_event::loop()
{
    fd_set read_fds{};
    while(true) {
        bool failed = false;
        int save_errno = errno;
        do {
            if(cbs_.empty()) {
                break;
            }
            FD_ZERO(&read_fds);
            for (auto && pair : cbs_) {
                auto fd = pair.first;
                FD_SET(fd, &read_fds);
            }
            int select_ret;
            errno = 0;
            if ((select_ret = select(get_max_fd() + 1, &read_fds, nullptr, nullptr, nullptr)) < 0) {
                if (errno != EINTR) {
                    if(on_select_error_) {
                        on_select_error_();
                    }
                    failed = true;
                    break;
                }
            }
        } while (errno == EINTR);
        errno = save_errno;

        if (failed) {
            break;
        }

        bool stop = false;

        // when callback calls remove_fd on actually iterating map we need to replace actual iterator with correct one
        // in the method ::remove_read_event
        actual_iterator_cbs_ = cbs_.begin();
        while (actual_iterator_cbs_ != cbs_.end()) {
            auto& pair = *actual_iterator_cbs_;
            const auto& fd = pair.first;
            const auto& cb = pair.second;
            if (FD_ISSET(fd, &read_fds)) {
                auto ret = cb(fd);
                if(ret == event_return::STOP_LOOP) {
                    stop = true;
                    break;
                }else if(ret == event_return::SKIP_OTHER_FD) {
                    break;
                }
            }
            if(!actual_iterator_deleted_) {
                ++actual_iterator_cbs_;
            } else {
                actual_iterator_deleted_ = false;
            }
        }
        if(stop) break;
    }
}

}