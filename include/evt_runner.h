//
// Created by Harold on 2020/8/13.
//

#ifndef DISPATCHER_EVT_RUNNER_H
#define DISPATCHER_EVT_RUNNER_H

#include <memory>
#include <thread>
#include <map>
#include <functional>
#include <chrono>
#include <vector>

//######################### helper ###########################
template<typename T>
struct is_chrono_duration {
    static constexpr bool value = false;
};

template<typename Rep, typename Period>
struct is_chrono_duration<std::chrono::duration<Rep, Period>> {
    static constexpr bool value = true;
};
//###################### end of helper ########################

template<typename EventType>
class evt_runner {
public:
    using callback_t = std::function<void(const EventType &)>;
    using defer_t = std::function<void(std::unique_lock<std::mutex> &)>;

private:
    using locker = std::unique_lock<std::mutex>;

public:
    evt_runner() : running_(false) {};
    // non-copyable
    evt_runner(const evt_runner &) = delete;
    evt_runner &operator=(const evt_runner &) = delete;
    // movable
    evt_runner(evt_runner &&) noexcept = default;

    void start();
    void pause();
    void stop();

    // register event and its callback
    void register_event(int event_id, callback_t function);
    void unregister_event(int event_id);

    // send event for immediate execution
    void send(int event_id, EventType evt) { post(event_id, std::move(evt), 0); }
    // post event for delayed execution, default duration is in milliseconds
    template<typename Duration = std::chrono::milliseconds>
    void post(int event_id, EventType evt, int duration_value = 10);

private:
    void loop();
    void defer(int event_id, EventType evt, locker &locker_);

private:
    std::atomic<bool> running_;
    std::thread thread_;
    std::mutex events_lock_;
    std::condition_variable events_condition_;
    std::multimap<int, callback_t> callbacks_;
    std::multimap<std::chrono::time_point<std::chrono::high_resolution_clock>, defer_t> events_;
};

template<typename EventType>
inline void evt_runner<EventType>::start() {
    running_ = true;
    thread_ = std::thread(&evt_runner<EventType>::loop, this);
}

template<typename EventType>
inline void evt_runner<EventType>::pause() {
    running_ = false;
    events_condition_.notify_one();
    thread_.join();
}

template<typename EventType>
inline void evt_runner<EventType>::stop() {
    running_ = false;
    events_condition_.notify_one();
    events_.clear();
    callbacks_.clear();
    thread_.join();
}

template<typename EventType>
inline void evt_runner<EventType>::register_event(int event_id, evt_runner::callback_t function) {
    locker _(events_lock_);
    callbacks_.insert(std::make_pair(event_id, std::move(function)));
}

template<typename EventType>
inline void evt_runner<EventType>::unregister_event(int event_id) {
    locker _(events_lock_);
    callbacks_.erase(event_id);
}

template<typename EventType>
template<typename Duration>
inline void evt_runner<EventType>::post(int event_id, EventType evt, int duration_value) {
    static_assert(is_chrono_duration<Duration>::value, "Duration must be a std::chrono::duration");
    auto duration = Duration(duration_value);
    {
        locker _(events_lock_);
        events_.insert(std::make_pair(std::move(std::chrono::high_resolution_clock::now() + duration),
                                      std::bind(&evt_runner::defer,
                                                this, event_id, std::move(evt), std::placeholders::_1)));
    }
    // wake up when new event coming
    events_condition_.notify_one();
}

template<typename EventType>
inline void evt_runner<EventType>::loop() {
    locker locker_(events_lock_);
    while (running_) {
        auto next_event = std::chrono::time_point<std::chrono::high_resolution_clock>::max();
        if (!events_.empty())
            next_event = events_.begin()->first;
        // wait until:
        // 1. new event coming
        // 2. terminate or event with smaller timestamp detected when refresh events
        events_condition_.wait_until(locker_, next_event, [&]() {
            return !running_ || (!events_.empty() && events_.begin()->first < next_event);
        });
        if (next_event <= std::chrono::high_resolution_clock::now()) {
            auto it = events_.begin();
            auto func = it->second;
            events_.erase(it);
            // temporarily releases lock
            func(locker_);
        }
    }
}

template<typename EventType>
inline void evt_runner<EventType>::defer(int event_id, EventType evt, evt_runner::locker &locker_) {
    // make a copy in case events changed
    auto range = callbacks_.equal_range(event_id);
    std::vector<callback_t> functions;
    for (auto it = range.first; it != range.second; it++) {
        functions.push_back(it->second);
    }
    // run copy without lock
    locker_.unlock();
    for (auto &function: functions) {
        function(evt);
    }
    // afterwards lock again
    locker_.lock();
}

#endif //DISPATCHER_EVT_RUNNER_H
