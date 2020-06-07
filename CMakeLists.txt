cmake_minimum_required(VERSION 3.15)
project(pexec)

set(CMAKE_CXX_STANDARD 11)

file(GLOB SOURCE_FILES FILES_MATCHING PATTERN "./src/*/*.cpp")
file(GLOB SOURCE_FILES_2 FILES_MATCHING PATTERN "./src/*/*/*.cpp")
add_library(pexec STATIC ${SOURCE_FILES} ${SOURCE_FILES_2})
target_include_directories(pexec PUBLIC src)

add_subdirectory(example)
