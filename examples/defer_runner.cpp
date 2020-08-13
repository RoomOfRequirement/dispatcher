//
// Created by Harold on 2020/8/13.
//

#include "defer_runner.h"
#include <cassert>
#include <iostream>
#include <chrono>

int main() {
    defer_runner dr;
    assert(dr.size() == 0);
    dr.start();
    auto t1 = dr.push([](){
        std::cout << "lambda task 1" << "\n";
    });
    t1.get();

    auto t2 = dr.push([](int x){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "lambda task 2: after 2 seconds, get " << x << "\n";
    }, 2);
    t2.get();

    for (int i = 0; i < 10; i ++) {
        dr.push([i](){ std::cout << "number: " << i << "\n"; });
    }
    auto task = dr.pop();
    std::cout << typeid(task).name() << "\n";
    task();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
