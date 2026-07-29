cmake_minimum_required(VERSION 4.2)
project(Gateway)

set(CMAKE_CXX_STANDARD 23)
add_executable(Gateway)
target_compile_definitions(Gateway PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        NOMINMAX
)

target_link_libraries(Gateway PRIVATE ws2_32)

add_subdirectory(source)
include_directories(include _3rd)