cmake_minimum_required(VERSION 3.15)
project(pexec_tests)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O0")

#TODO: add test framework, gtest etc..

find_package(Threads REQUIRED)

add_executable(pexec_benchmark_test benchmark.cpp)
target_link_libraries(pexec_benchmark_test pexec Threads::Threads)

add_executable(pexec_fd_test fd.cpp)
target_link_libraries(pexec_fd_test pexec)

add_executable(pexec_arg_parsing_test arg_parsing.cpp)
target_link_libraries(pexec_arg_parsing_test pexec)