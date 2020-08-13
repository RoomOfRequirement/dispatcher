//
// Created by Harold on 2020/8/13.
//

#include "task_group.h"
#include <iostream>

void f(int x) {
    std::cout << "call f with x = " << x << "\n";
}

int main() {
    task_group tg(2);
    assert(tg.size() == 2);
    tg.start();
    assert(tg.waiting_tasks() == 0);

    tg.send(std::ref(f), 1);

    tg.send([](){
        std::cout << "send simple lambda\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        struct A {
            static void g() {
                std::cout << "static member function\n";
            }
            void h() const {
                std::cout << "member function, a = "<< a << "\n";
            }
            int a;
        };
        auto ts = task_runner::now() + std::chrono::milliseconds(500);
        tg.push(A::g, ts);
        A a{100};
        tg.push(std::bind(&A::h, &a), ts);
        assert(tg.waiting_tasks() == 2);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    tg.stop();

    tg.start();

    tg.push([](int x){
        std::cout << "restart here: " << x << std::endl;
    }, task_runner::now(), 2);

    return 0;
}
