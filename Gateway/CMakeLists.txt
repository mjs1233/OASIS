cmake_minimum_required(VERSION 4.2)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)

add_executable(Gateway)
add_subdirectory(source)
include_directories(include _3rd)