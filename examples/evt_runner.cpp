//
// Created by Harold on 2020/8/13.
//

#include "evt_runner.h"
#include <iostream>

struct event {
    evt_runner<event>& runner;
    static const int id = 0;
};

int main() {
    evt_runner<event> runner;
    runner.register_event(event::id, [](const event& evt) {
        std::cout << "repeated event run after 1000 milli-seconds\n";
        // repeat
        evt.runner.post(event::id, evt, 1000);
    });
    runner.start();
    auto start = std::chrono::high_resolution_clock::now();
    event evt{runner};
    runner.post(event::id, evt, 1000);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    runner.pause();
    std::cout << "time consumption: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start).count()
              << "ms"
              << std::endl;

    std::cout << "-------------------------------------------" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // former is pause, so here need unregister first
    runner.unregister_event(event::id);
    runner.register_event(event::id, [](const event& evt) {
        std::cout << "repeated event run after 2 seconds\n";
        // change duration base to seconds
        evt.runner.post<std::chrono::seconds>(event::id, evt, 2);
    });
    runner.start();
    start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    runner.stop();
    std::cout << "time consumption: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start).count()
              << "ms"
              << std::endl;

    std::cout << "-------------------------------------------" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // former is stop, no need to unregister
    runner.register_event(event::id, [](const event& evt) {
        std::cout << "call once" << std::endl;
    });
    runner.start();
    runner.send(event::id, evt);
    start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    runner.stop();
    std::cout << "time consumption: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start).count()
              << "ms"
              << std::endl;

    return 0;
}
