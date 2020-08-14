cmake_minimum_required(VERSION 3.17)
project(dispatcher)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)

function(add_exe name folder)
    add_executable(${name} ${folder}/${name}.cpp)
endfunction()

# examples
add_exe(task_runner examples)
add_exe(task_group examples)
add_exe(evt_runner examples)
add_exe(defer_runner examples)
add_exe(defer_pool examples)

# toy
add_exe(task_pool toy)
add_exe(_wsq toy/wsq)