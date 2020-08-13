cmake_minimum_required(VERSION 3.17)
project(dispatcher)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)

function(addExample name)
    add_executable(${name} examples/${name}.cpp)
endfunction()

addExample(task_runner)
addExample(task_group)
addExample(evt_runner)
addExample(defer_runner)
addExample(defer_pool)