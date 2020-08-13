//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_DEFER_POOL_H
#define DISPATCHER_DEFER_POOL_H

#include "safe_queue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <future>

class defer_pool {
public:
    using task_t = std::function<void()>;
private:
    using flag_t = std::atomic<bool>;
    using locker = std::unique_lock<std::mutex>;

public:
    defer_pool() : tasks_done_(false), pool_stop_(false), n_idle(0) { };
    explicit defer_pool(size_t n_threads);
    // non-copyable
    defer_pool(const defer_pool &) = delete;
    defer_pool& operator=(const defer_pool &) = delete;
    // non-movable
    defer_pool(defer_pool &&) = delete;
    defer_pool& operator=(defer_pool &&) = delete;
    ~defer_pool() { stop(true); }

    void stop(bool wait = false);
    void clear_tasks();

    inline size_t size() const { return threads_.size(); }
    inline size_t idle_threads() const { return n_idle; }

    void resize(size_t n_threads);

    template<typename F, typename ...Args>
    auto push(F&& f, Args&& ...args) -> std::future<decltype(f(args...))>;
    template<typename F>
    auto push(F&& f) -> std::future<decltype(f())>;
    task_t pop();

private:
    void setup_thread(size_t i);

private:
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::vector<std::shared_ptr<flag_t>> threads_stop_flags_;
    safe_queue<task_t*> tasks_;
    std::mutex lock_;
    std::condition_variable condition_;
    flag_t tasks_done_;
    flag_t pool_stop_;
    std::atomic<int> n_idle;
};

inline defer_pool::defer_pool(size_t n_threads) : defer_pool() {
    threads_.resize(n_threads);
    threads_stop_flags_.resize(n_threads);
    for (size_t i = 0; i < n_threads; i++) {
        threads_stop_flags_[i] = std::make_shared<flag_t>(false);
        setup_thread(i);
    }
}

// stop accepting new tasks and wait for all tasks done
inline void defer_pool::stop(bool wait) {
    if (!wait) {
        if (pool_stop_)
            return;
        pool_stop_ = true;
        for (size_t i = 0, n = threads_.size(); i < n; i++)
            *threads_stop_flags_[i] = true;
        // clear tasks to avoid idle threads fetch task
        clear_tasks();
    } else {
        if (tasks_done_ || pool_stop_)
            return;
        tasks_done_ = true;
    }
    {
        locker _(lock_);
        // notify all waiting threads to stop
        condition_.notify_all();
    }
    // wait for finishing running task
    for (auto & thread : threads_)
        if (thread->joinable())
            thread->join();
    clear_tasks();
    threads_.clear();
    threads_stop_flags_.clear();
}

inline void defer_pool::clear_tasks() {
    task_t* tp = nullptr;
    while (tasks_.pop(tp))
        delete tp;
}

inline void defer_pool::resize(size_t n_threads) {
    if (!pool_stop_ && !tasks_done_) {
        size_t old_n_threads = threads_.size();
        // expand
        if (old_n_threads <= n_threads) {
            threads_.resize(n_threads);
            threads_stop_flags_.resize(n_threads);
            for (size_t i = old_n_threads; i < n_threads; i++) {
                threads_stop_flags_[i] = std::make_shared<flag_t>(false);
                setup_thread(i);
            }
        } else {
            // shrink
            for (size_t i = old_n_threads - 1; i >= n_threads; i--) {
                *threads_stop_flags_[i] = true;
                // detach first to let running task complete
                threads_[i]->detach();
            }
            {
                locker _(lock_);
                // notify detached thread to stop
                condition_.notify_all();
            }
            threads_.resize(n_threads);
            threads_stop_flags_.resize(n_threads);
        }
    }
}

template<typename F, typename ...Args>
inline auto defer_pool::push(F&& f, Args&& ...args)
    -> std::future<decltype(f(args...))> {
    auto pck = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto tp = new task_t([pck](){ (*pck)(); });
    tasks_.push(tp);
    locker _(lock_);
    condition_.notify_one();
    return pck->get_future();
}

template<typename F>
inline auto defer_pool::push(F&& f)
    -> std::future<decltype(f())> {
    auto pck = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
    auto tp = new task_t([pck](){ (*pck)(); });
    tasks_.push(tp);
    locker _(lock_);
    condition_.notify_one();
    return pck->get_future();
}

inline defer_pool::task_t defer_pool::pop() {
    task_t* tp = nullptr;
    tasks_.pop(tp);
    task_t task;
    std::unique_ptr<task_t> utp(tp);
    if (tp)
        task = *tp;
    return task;
}

inline void defer_pool::setup_thread(size_t i)  {
    std::shared_ptr<flag_t> fp(threads_stop_flags_[i]);
    auto loop_f = [this, fp]() {
        flag_t& stop = *fp;
        task_t* tp = nullptr;
        bool has_next = tasks_.pop(tp);
        while (true) {
            while (has_next) {
                // drop task function after execution
                std::unique_ptr<task_t> utp(tp);
                // execute task
                (*tp)();
                // if stop flag set, then stop loop
                if (stop)
                    return;
                // fetch next task
                has_next = tasks_.pop(tp);
            }
            // no tasks now
            locker locker_(lock_);
            ++n_idle;
            // check whether new task coming or stop
            condition_.wait(locker_, [this, &tp, &has_next, &stop](){
                has_next = tasks_.pop(tp);
                return stop || tasks_done_ || has_next;
            });
            --n_idle;
            // no new task coming, then stop
            if (!has_next)
                return;
        }
    };
    // bind loop_f to thread
    threads_[i].reset(new std::thread(loop_f));
}

#endif //DISPATCHER_DEFER_POOL_H
