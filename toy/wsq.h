//
// Created by Harold on 2020/8/14.
//

#ifndef DISPATCHER_WSQ_H
#define DISPATCHER_WSQ_H

#include <queue>
#include <mutex>

// simple toy work steal queue
template<typename T>
class wsq {
private:
    using locker = std::unique_lock<std::mutex>;

public:
    wsq(): q_(), lock_() {}
    bool empty() {
        locker _(lock_);
        return q_.empty();
    }
    // push at front
    bool push(const T& v) {
        locker _(lock_);
        q_.push_front(v);
        return true;
    }
    // pop at front
    bool pop(T& v) {
        locker _(lock_);
        if (q_.empty())
            return false;
        v = q_.front();
        q_.pop_front();
        return true;
    }
    // steal at back
    bool steal(T& v) {
        locker locker_(lock_, std::try_to_lock);
        if (!locker_ || q_.empty())
            return false;
        v = q_.back();
        q_.pop_back();
        return true;
    }

private:
    std::deque<T> q_;
    std::mutex lock_;
};

#endif //DISPATCHER_WSQ_H
