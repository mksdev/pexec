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

find_package(Threads REQUIRED)
add_executable(pexec_multi_thread_example multi_thread.cpp)
target_link_libraries(pexec_multi_thread_example pexec Threads::Threads)

add_executable(pexec_multi_detail multi_detail.cpp)
target_link_libraries(pexec_multi_detail pexec)


find_package(Libevent)
#  Libevent_FOUND        - True if headers and requested libraries were found
#  Libevent_INCLUDE_DIRS - Libevent include directories
#  Libevent_LIBRARIES    - Libevent libraries to be linked
message(STATUS "Libevent_FOUND: ${Libevent_FOUND}")
message(STATUS "Libevent_INCLUDE_DIRS: ${Libevent_INCLUDE_DIRS}")
message(STATUS "Libevent_LIBRARIES: ${Libevent_LIBRARIES}")
message("")

if(Libevent_FOUND)
    add_executable(pexec_multi_libevent2 multi_libevent2.cpp)

    target_include_directories(pexec_multi_libevent2 PUBLIC
            ${Libevent_INCLUDE_DIRS})

    target_link_libraries(pexec_multi_libevent2
            ${Libevent_LIBRARIES} pexec)
endif()
