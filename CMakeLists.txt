cmake_minimum_required(VERSION 3.15)
project(pexec)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

file(GLOB_RECURSE SOURCE_FILES FILES_MATCHING PATTERN "./src/*.cpp")
add_library(pexec STATIC ${SOURCE_FILES})
target_include_directories(pexec PUBLIC src)

add_subdirectory(example)
add_subdirectory(test)