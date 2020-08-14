//
// Created by Harold on 2020/8/14.
//

#ifndef DISPATCHER_TASK_POOL_H
#define DISPATCHER_TASK_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>
#include <cstdio>

#ifdef USE_SIMPLE_QUEUE
#include "wsq.h"
#else
#include "wsq/_wsq.h"
template<typename T>
using wsq = WorkStealingQueue<T>;
#endif

class task_pool {
public:
    using task_t = std::function<void()>;
    using flag_t = std::atomic<bool>;

public:
    explicit task_pool(size_t n_threads): next_idx_(0), qs_(n_threads), n_threads_(n_threads), n_waiting_tasks_(0) {
        assert(n_threads > 0);
        threads_.reserve(n_threads);
        threads_stop_flags_.resize(n_threads);
        for (auto i = 0; i < n_threads; i++) {
            threads_stop_flags_[i] = std::make_shared<flag_t>(false);
            threads_.emplace_back([&, i](){loop_f(i);});
        }
    }
    ~task_pool() {
        // simply wait for all tasks done
        while (n_waiting_tasks_ != 0) { }
        for (auto i = 0; i < n_threads_; i++) {
            *threads_stop_flags_[i] = true;
        }
        for (auto & thread : threads_)
            thread.join();
    }
    template<typename F>
    void push(F&& f) {
        ++n_waiting_tasks_;
        qs_[next_idx_++ % n_threads_].push(new task_t(std::forward<F>(f)));
    }

private:
    std::vector<std::thread> threads_;
    std::vector<std::shared_ptr<flag_t>> threads_stop_flags_;
    std::vector<wsq<task_t*>> qs_;
    std::atomic<size_t> next_idx_;
    size_t const n_threads_;
    std::atomic<size_t> n_waiting_tasks_;

private:
    void loop_f(size_t i) {
        while (!*threads_stop_flags_[i]) {
            task_t* tp = nullptr;
#ifdef USE_SIMPLE_QUEUE
            // _wsq requires push and pop in the same thread
            // my simple wsq based on lock is ok for both
            // fetch task from its own queue
            if (qs_[i].pop(tp) && tp) {
                (*tp)();
                --n_waiting_tasks_;
                delete tp;
                continue;
            }
#endif
            // steal task from other threads
            for (auto j = 0; j < n_threads_; j++)
                if (qs_[(i+j) % n_threads_].steal(tp) && tp) {
                    printf("%zu steal from %lu\n", i, (i+j) % n_threads_);
                    (*tp)();
                    --n_waiting_tasks_;
                    delete tp;
                    break;
                }
        }
    }
};

#endif //DISPATCHER_TASK_POOL_H
