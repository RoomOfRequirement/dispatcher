//
// Created by Harold on 2020/8/14.
//

#include <thread>
#include "_wsq.h"

int main() {
    // work-stealing queue of integer numbers
    WorkStealingQueue<int> queue;

    // only one thread can push and pop items from one end
    std::thread owner([&] () {
        for(int i=0; i<1000; i++)
            queue.push(i);
        int item;
        int cnt = 0;
        while(!queue.empty())
            if (queue.pop(item)) {
                ++cnt;
                //printf("item popped: %d\n", item);
            }
        printf("item num popped: %d\n", cnt);
    });

    // multiple threads can steal items from the other end
    std::thread thief1([&] () {
        int item;
        int cnt = 0;
        while(!queue.empty())
            if (queue.steal(item)) {
                ++cnt;
                //printf("item stolen from 1: %d\n", item);
            }
        printf("item num stolen from 1: %d\n", cnt);
    });
    std::thread thief2([&] () {
        int item;
        int cnt = 0;
        while(!queue.empty())
            if (queue.steal(item)) {
                ++cnt;
                //printf("item stolen from 1: %d\n", item);
            }
        printf("item num stolen from 2: %d\n", cnt);
    });

    owner.join();
    thief1.join();
    thief2.join();

    return 0;
}
