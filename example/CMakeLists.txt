cmake_minimum_required(VERSION 3.15)
project(pexec_examples)

set(CMAKE_CXX_STANDARD 11)

add_executable(pexec_simple main.cpp)
target_link_libraries(pexec_simple pexec)

add_executable(pexec_benchmark benchmark.cpp)
target_link_libraries(pexec_benchmark pexec)
