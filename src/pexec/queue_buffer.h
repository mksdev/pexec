//
// Created by michalks on 28.07.19.
//

#ifndef PEXEC_QUEUEBUFFER_H
#define PEXEC_QUEUEBUFFER_H

#include <deque>
#include <condition_variable>

namespace pexec {

template<typename T, typename Alloc = std::allocator<T>, template<typename U = T, typename A = Alloc> class V = std::deque>
class queue_buffer {

public:
    void add(const T& num) {
        std::unique_lock<std::mutex> locker(mu_);
        cond_.wait(locker, [this](){return buffer_.size() < size_;});
        buffer_.push_back(num);
        locker.unlock();
        cond_.notify_one();
    }

    void clear() {
        std::unique_lock<std::mutex> locker(mu_);
        buffer_.clear();
        locker.unlock();
        cond_.notify_one();
    }

    void clear_add(const T& val) {
        std::unique_lock<std::mutex> locker(mu_);
        buffer_.clear();
        buffer_.push_back(val);
        locker.unlock();
        cond_.notify_one();
    }

    T remove() {
        std::unique_lock<std::mutex> locker(mu_);
        cond_.wait(locker, [this](){return buffer_.size() > 0;});
        T back = buffer_.front();
        buffer_.pop_front();
        locker.unlock();
        cond_.notify_one();
        return back;
    }

    std::vector<T> state() {
        std::unique_lock<std::mutex> locker(mu_);
        std::vector<T> copy;
        for(const auto& it : buffer_) {
            copy.push_back(it);
        }
        return copy;
    }

    queue_buffer() = default;

private:
    // Add them as member variables here
    std::mutex mu_;
    std::condition_variable cond_;

    // Your normal variables here
    V<T, Alloc> buffer_;
    const unsigned long size_ = std::numeric_limits<unsigned long>::max();

};

}
#endif //PEXEC_QUEUEBUFFER_H
