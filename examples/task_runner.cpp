//
// Created by Harold on 2020/8/13.
//

#include "task_runner.h"
#include <iostream>

void f(int x) {
    std::cout << "call f with x = " << x << "\n";
}

int main() {
    task_runner tr(task_runner::stop_mode::WAIT_ALL_DONE);
    assert(tr.waiting_tasks() == 0);

    tr.start();

    tr.send(std::ref(f), 1);

    tr.send([](){
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
        tr.push(A::g, ts);
        A a{100};
        tr.push(std::bind(&A::h, &a), ts);
        assert(tr.waiting_tasks() == 2);
    }

    tr.stop();

    tr.start();

    tr.push([](int x){
        std::cout << "restart here: " << x << std::endl;
    }, task_runner::now(), 2);

    return 0;
}
