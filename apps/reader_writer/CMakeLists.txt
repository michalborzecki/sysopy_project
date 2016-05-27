cmake_minimum_required(VERSION 3.4)

set(CMAKE_C_FLAGS "-Wall -lrt -pthread")

add_executable(rw_main main.c)
add_executable(rw_writer writer.c)
add_executable(rw_reader reader.c)
