cmake_minimum_required(VERSION 3.15)
project(pexec)

set(CMAKE_CXX_STANDARD 14)

file(GLOB SOURCE_FILES FILES_MATCHING PATTERN "./src/*/*.cpp")

add_library(pexec STATIC ${SOURCE_FILES})
target_include_directories(pexec PUBLIC src)

add_executable(fork_watch
    main.cpp
)
target_link_libraries(fork_watch pexec)

add_executable(simple_echo
    simple_echo.cpp
)
add_executable(simple_in
    simple_in.cpp
)