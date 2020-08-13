//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_TASK_GROUP_H
#define DISPATCHER_TASK_GROUP_H

#include "task_runner.h"
#include <vector>

class task_group {
public:
    enum task_forward_strategy {
        ROUND_ROBIN,  // simple round-robin
        LEAST_TASKS   // forward to thread has least tasks
    };
    using time_stamp = task_runner::time_stamp;
    using stop_mode = task_runner::stop_mode;

public:
    explicit task_group(size_t n_threads,
                        stop_mode sm = stop_mode::WAIT_CURRENT_DONE,
                        task_forward_strategy strategy = ROUND_ROBIN);
    // non-copyable
    task_group(const task_group &) = delete;
    task_group& operator=(const task_group &) = delete;
    // non-movable
    task_group(task_group &&) = delete;
    task_group& operator=(task_group &&) = delete;
    ~task_group() { stop(); }

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

    size_t size() { return runners.size(); }
    size_t waiting_tasks();

private:
    int next_to();

private:
    task_forward_strategy strategy_;
    size_t next_idx_;
    std::vector<std::unique_ptr<task_runner>> runners;
};

inline task_group::task_group(size_t n_threads,
                              stop_mode sm,
                              task_forward_strategy strategy)
        : strategy_(strategy), next_idx_(0) {
    assert(n_threads > 0);
    runners.resize(n_threads);
    for (auto i = 0; i < n_threads; i++)
        runners[i].reset(new task_runner(sm));
}

inline void task_group::start() {
    for (auto & runner : runners)
        runner->start();
}

inline void task_group::stop() {
    for (auto & runner : runners)
        runner->stop();
}

template<typename F, typename... Args>
inline void task_group::push(F &&f, time_stamp ts, Args &&... args) {
    runners[next_to()]->push(std::forward<F>(f),
                             std::forward<time_stamp>(ts),
                             std::forward<Args>(args)...);
}

template<typename F>
inline void task_group::push(F &&f, time_stamp ts) {
    runners[next_to()]->push(std::forward<F>(f),
                             std::forward<time_stamp>(ts));
}

template<typename F, typename... Args>
inline void task_group::send(F &&f, Args &&... args) {
    runners[next_to()]->send(std::forward<F>(f),
                             std::forward<Args>(args)...);
}

template<typename F>
inline void task_group::send(F &&f) {
    runners[next_to()]->send(std::forward<F>(f));
}

inline int task_group::next_to() {
    auto n_runners = runners.size();
    switch (strategy_) {
        case ROUND_ROBIN:
            ++next_idx_;
            return next_idx_ % n_runners;
        case LEAST_TASKS:
            size_t idx = 0;
            size_t min = 0;
            for (auto i = 0; i < n_runners; i++)
                if (min > runners[i]->waiting_tasks()) {
                    min = runners[i]->waiting_tasks();
                    idx = i;
                }
            return idx;
    }
}

inline size_t task_group::waiting_tasks() {
    size_t sum = 0;
    for (auto & runner : runners)
        sum += runner->waiting_tasks();
    return sum;
}

#endif //DISPATCHER_TASK_GROUP_H
