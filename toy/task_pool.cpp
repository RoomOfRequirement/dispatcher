//
// Created by Harold on 2020/8/14.
//

#define USE_SIMPLE_QUEUE

#include "task_pool.h"
#include <chrono>

int main() {
    std::vector<int> test;
    test.resize(100000);
    auto start = std::chrono::high_resolution_clock::now();
    {
        task_pool tp(3);
        for (int & e : test) {
            tp.push([&e]() { e++; });
        }
    }
    printf("tasks done in %lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>
                   (std::chrono::high_resolution_clock::now() - start).count());
    // 80ms for wsq, 400ms for _wsq

    for (int i = 0; i < test.size(); i++) {
        if (test[i] != 1)
            printf("ERROR: idx %d expected 1 but got %d\n", i, test[i]);
    }

    return 0;
}