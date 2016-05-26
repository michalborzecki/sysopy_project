cmake_minimum_required(VERSION 3.4)

set(CMAKE_C_FLAGS "-Wall -pthread")

add_executable(philosophers_main main.c)
