//
// Created by Harold on 2020/8/13.
//

#include "defer_pool.h"
#include <iostream>
#include <string>

void f(int x) {
    std::cout << "call f with x = " << x << "\n";
}

int main(int argc, char **argv) {
    defer_pool p(2);
    assert(p.size() == 2);
    std::cout << "size: " << p.size() << "\n";
    std::cout << "idle: " << p.idle_threads() << "\n";

    std::future<void> qw = p.push(std::ref(f), 1);

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
        auto t = p.push(A::g);
        t.get();
        A a{100};
        auto t1 = p.push(std::bind(&A::h, &a));
        t1.get();
    }

    std::string s = "lambda";
    p.push([s](int x){  // lambda
        std::this_thread::sleep_for(std::chrono::seconds (2));
        std::cout << "hello from " << s << " with x = " << x << '\n';
    }, 2);

    p.push([s](int x){  // lambda
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        std::cout << "hello from " << s << " with x = " << x << '\n';
    }, 3);

    auto f = p.pop();
    if (f) {
        std::cout << "pop function from the pool\n";
        f();
    }

    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds (5));

    std::cout << "after sleep idle becomes: " << p.idle_threads() << std::endl;
    p.resize(1);
    assert(p.size() == 1);

    std::cout << "size becomes: " << p.size() << std::endl;
    std::cout << "idle becomes: " << p.idle_threads() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds (2));

    auto f1 = p.push([](){
        throw std::exception();
    });
    try {
        f1.get();
    }
    catch (std::exception & e) {
        std::cout << "throw exception in pushed function and catch it in get()\n";
    }

    auto f2 = p.push([](){
        std::cout << "after exception, pool sill works";
    });
    f2.get();

    std::cout << std::endl;

    return 0;
}
