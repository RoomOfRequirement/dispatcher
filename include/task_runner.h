//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_TASK_RUNNER_H
#define DISPATCHER_TASK_RUNNER_H

#include <functional>
#include <atomic>
#include <thread>
#include <map>
#include <queue>
#include <chrono>
#include <mutex>

class task_runner {
public:
    enum class stop_mode {
        IMMEDIATE,
        WAIT_CURRENT_DONE,
        WAIT_ALL_DONE
    };
    using time_stamp = std::chrono::time_point<std::chrono::system_clock,
            std::chrono::microseconds>;
    static time_stamp now() {
        return std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    }

public:
    explicit task_runner(stop_mode sm = stop_mode::IMMEDIATE);
    // non-copyable
    task_runner(const task_runner &) = delete;
    task_runner& operator=(const task_runner &) = delete;
    // non-movable
    task_runner(task_runner &&) = delete;
    task_runner& operator=(task_runner &&) = delete;
    ~task_runner() { stop(); }

    void start();
    void stop();

    // push task for delayed execution (insert into deferred_tasks_)
    template<typename F, typename ...Args>
    void push(F&& f, time_stamp ts, Args&& ...args);
    template<typename F>
    void push(F&& f, time_stamp ts);

    // send task for immediate execution (push into tasks_)
    template<typename F, typename ...Args>
    void send(F&& f, Args&& ...args);
    template<typename F>
    void send(F&& f);

    size_t waiting_tasks() { return n_waiting_tasks_; };

private:
    void loop_f();

private:
    using task_t = std::function<void()>;
    using flat_t = std::atomic<bool>;
    using locker = std::unique_lock<std::mutex>;

    stop_mode stop_mode_;
    flat_t running_;
    std::unique_ptr<std::thread> thread_;
    std::multimap<time_stamp, task_t*> deferred_tasks_;
    std::queue<task_t*> tasks_;  // push deferred_tasks into tasks_ when time arrived
    std::atomic<size_t> n_waiting_tasks_;  // n_waiting_tasks = tasks + deferred_tasks
    std::mutex task_lock_;
    std::condition_variable condition_;
};

inline task_runner::task_runner(stop_mode sm)
        : running_(false), stop_mode_(sm), n_waiting_tasks_(0) { }

inline void task_runner::start() {
    running_ = true;
    thread_.reset(new std::thread(&task_runner::loop_f, this));
}

inline void task_runner::stop() {
    running_ = false;
    if (thread_->joinable())
        thread_->join();
}

template<typename F, typename ...Args>
inline void task_runner::push(F&& f, task_runner::time_stamp ts, Args&& ...args) {
    if (ts <= now())
        return send(std::forward<F>(f), std::forward<Args>(args)...);
    ++n_waiting_tasks_;
    auto task = new task_t(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    locker _(task_lock_);
    deferred_tasks_.insert(std::pair<time_stamp, task_t*>(ts, task));
    condition_.notify_one();
}

template<typename F>
inline void task_runner::push(F&& f, task_runner::time_stamp ts) {
    if (ts <= now())
        return send(std::forward<F>(f));
    ++n_waiting_tasks_;
    auto task = new task_t(std::forward<F>(f));
    locker _(task_lock_);
    deferred_tasks_.insert(std::pair<time_stamp, task_t*>(ts, task));
    condition_.notify_one();
}

template<typename F, typename ...Args>
inline void task_runner::send(F&& f, Args&& ...args) {
    ++n_waiting_tasks_;
    auto task = new task_t(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    locker _(task_lock_);
    tasks_.push(task);
    condition_.notify_one();
}
template<typename F>
inline void task_runner::send(F&& f) {
    ++n_waiting_tasks_;
    auto task = new task_t(std::forward<F>(f));
    locker _(task_lock_);
    tasks_.push(task);
    condition_.notify_one();
}

inline void task_runner::loop_f() {
    std::queue<task_t*> ready_to_execute_tasks;
    while (running_) {
        {
            locker locker_(task_lock_);
            while (running_ && tasks_.empty()) {
                // no immediate tasks, sleep 100ms
                condition_.wait_for(locker_, std::chrono::milliseconds(100));
                if (!deferred_tasks_.empty())
                    break;
            }
            // collect immediate tasks
            if (!tasks_.empty())
                ready_to_execute_tasks.swap(tasks_);
            // collect deferred tasks
            auto next_event = time_stamp::max();
            if (!deferred_tasks_.empty())
                next_event = deferred_tasks_.begin()->first;
            auto now_ = now();
            if (next_event <= now_) {
                auto it = deferred_tasks_.begin();
                for (; it != deferred_tasks_.end(); ++it) {
                    if (it->first > now_)
                        break;
                    ready_to_execute_tasks.push(it->second);
                }
                deferred_tasks_.erase(deferred_tasks_.begin(), it);
            }
        }
        // execute tasks
        // not execute if stop triggered (!running_)
        while (running_ && !ready_to_execute_tasks.empty()) {
            std::unique_ptr<task_t> tp(ready_to_execute_tasks.front());
            ready_to_execute_tasks.pop();
            --n_waiting_tasks_;
            (*tp)();
        }
    }
    // cleanup
    // lock to not accept task any more
    locker _(task_lock_);
    switch (stop_mode_) {
        case stop_mode::IMMEDIATE:
            break;
        case stop_mode::WAIT_CURRENT_DONE:
            while (!ready_to_execute_tasks.empty()) {
                std::unique_ptr<task_t> tp(ready_to_execute_tasks.front());
                ready_to_execute_tasks.pop();
                --n_waiting_tasks_;
                (*tp)();
            }
            break;
        case stop_mode::WAIT_ALL_DONE:
            // collect all deferred tasks
            for (auto & deferred_task : deferred_tasks_)
                ready_to_execute_tasks.push(deferred_task.second);
            deferred_tasks_.clear();
            while (!ready_to_execute_tasks.empty()) {
                std::unique_ptr<task_t> tp(ready_to_execute_tasks.front());
                ready_to_execute_tasks.pop();
                --n_waiting_tasks_;
                (*tp)();
            }
            break;
    }
}

#endif //DISPATCHER_TASK_RUNNER_H
