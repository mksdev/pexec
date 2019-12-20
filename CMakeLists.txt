cmake_minimum_required(VERSION 3.15)
project(fork_watch)

set(CMAKE_CXX_STANDARD 14)

add_executable(fork_watch
    main.cpp
    argument_parser.cpp
)
add_executable(simple_echo
    simple_echo.cpp
)

add_executable(simple_in
    simple_in.cpp
)