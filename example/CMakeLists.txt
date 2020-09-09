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


find_package(LIBEVENT)
message(STATUS "LIBEVENT_FOUND: ${LIBEVENT_FOUND}")
message(STATUS "LIBEVENT_INCLUDE_DIRS: ${LIBEVENT_INCLUDE_DIRS}")
message(STATUS "LIBEVENT_LIBRARIES: ${LIBEVENT_LIBRARIES}")
message("")

if(LIBEVENT_FOUND)
    add_executable(pexec_multi_libevent2 multi_libevent2.cpp)

    target_include_directories(pexec_multi_libevent2 PUBLIC
            ${LIBEVENT_INCLUDE_DIRS})

    target_link_libraries(pexec_multi_libevent2
            ${LIBEVENT_LIBRARIES} pexec)
endif()
