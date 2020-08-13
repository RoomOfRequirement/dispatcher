//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_SAFE_QUEUE_H
#define DISPATCHER_SAFE_QUEUE_H

#include <queue>
#include <mutex>

template<typename T>
class safe_queue {
private:
    using locker = std::unique_lock<std::mutex>;

public:
    safe_queue(): q_(), lock_() {}
    bool empty() {
        locker _(lock_);
        return q_.empty();
    }
    bool push(const T& v) {
        locker _(lock_);
        q_.push(v);
        return true;
    }
    bool pop(T& v) {
        locker _(lock_);
        if (q_.empty())
            return false;
        v = q_.front();
        q_.pop();
        return true;
    }

private:
    std::queue<T> q_;
    std::mutex lock_;
};

#endif //DISPATCHER_SAFE_QUEUE_H
