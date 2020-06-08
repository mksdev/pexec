cmake_minimum_required(VERSION 3.15)
project(pexec_examples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O0")

add_executable(pexec_single_example single.cpp)
target_link_libraries(pexec_single_example pexec)

add_executable(pexec_multi_example multi.cpp)
target_link_libraries(pexec_multi_example pexec)

add_executable(pexec_multi_test_example multi_test.cpp)
target_link_libraries(pexec_multi_test_example pexec)

add_executable(pexec_multi_thread_example multi_thread.cpp)
target_link_libraries(pexec_multi_thread_example pexec)