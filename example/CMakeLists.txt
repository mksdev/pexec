cmake_minimum_required(VERSION 3.15)
project(pexec_examples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O0")
add_executable(pexec_simple main.cpp)
target_link_libraries(pexec_simple pexec)

add_executable(pexec_benchmark benchmark.cpp)
target_link_libraries(pexec_benchmark pexec)

add_executable(pexec_test_fd test_fd.cpp)
target_link_libraries(pexec_test_fd pexec)

add_executable(pexec_multi multi_test.cpp)
target_link_libraries(pexec_multi pexec)

add_executable(pexec_multi_example multi_example.cpp)
target_link_libraries(pexec_multi_example pexec)

add_executable(pexec_multi_thread_example multi_thread_example.cpp)
target_link_libraries(pexec_multi_thread_example pexec)