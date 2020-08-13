//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_DEFER_RUNNER_H
#define DISPATCHER_DEFER_RUNNER_H

#include <mutex>
#include <functional>
#include <queue>
#include <atomic>
#include <future>
#include <thread>

class defer_runner {
public:
    using task_t = std::function<void()>;

private:
    using locker = std::unique_lock<std::mutex>;

public:
    defer_runner() : running_(false), tasks_() { }
    // non-copyable
    defer_runner(const defer_runner &) = delete;
    defer_runner &operator=(const defer_runner &) = delete;
    // non-movable
    defer_runner(defer_runner &&) noexcept = delete;
    ~defer_runner() { stop(); }

    void start();
    void stop();

    template<typename F, typename ...Args>
    auto push(F&& f, Args&& ...args) -> std::future<decltype(f(args...))>;
    template<typename F>
    auto push(F&& f) -> std::future<decltype(f())>;
    task_t pop();

    void clear_tasks();

    size_t size() {
        locker _(lock_);
        return tasks_.size();
    }

private:
    void loop();

private:
    std::mutex lock_;
    std::condition_variable condition_;
    std::queue<task_t*> tasks_;
    std::atomic<bool> running_;
    std::thread thread_;
};

inline void defer_runner::start() {
    running_ = true;
    thread_ = std::thread(&defer_runner::loop, this);
}

inline void defer_runner::stop() {
    running_ = false;
    clear_tasks();
    condition_.notify_one();
    thread_.join();
}

template<typename F, typename ...Args>
inline auto defer_runner::push(F&& f, Args&& ...args)
    -> std::future<decltype(f(args...))> {
    auto pck = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto task = new task_t([pck](){ (*pck)(); });
    locker _(lock_);
    tasks_.push(task);
    condition_.notify_one();
    return pck->get_future();
}

template<typename F>
inline auto defer_runner::push(F&& f)
    -> std::future<decltype(f())> {
    auto pck = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
    auto tp = new task_t([pck](){ (*pck)(); });
    locker _(lock_);
    tasks_.push(tp);
    condition_.notify_one();
    return pck->get_future();
}

inline defer_runner::task_t defer_runner::pop() {
    locker _(lock_);
    task_t task;
    if (!tasks_.empty()) {
        std::unique_ptr<task_t> tp(tasks_.front());
        tasks_.pop();
        if (tp)
            task = *tp;
    }
    return task;
}

inline void defer_runner::clear_tasks() {
    locker _(lock_);
    task_t* tp;
    while (true) {
        tp = tasks_.front();
        if (tp) {
            tasks_.pop();
            delete tp;
        } else return;
    }
}

inline void defer_runner::loop() {
    locker locker_(lock_, std::defer_lock);
    while (running_) {
        locker_.lock();
        condition_.wait(locker_, [this]() {
            return !running_ || !tasks_.empty();
        });
        if (!tasks_.empty()) {
            std::unique_ptr<task_t> tp(tasks_.front());
            tasks_.pop();
            locker_.unlock();
            (*tp)();
        }
    }
}

#endif //DISPATCHER_DEFER_RUNNER_H
