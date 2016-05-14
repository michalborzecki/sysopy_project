cmake_minimum_required(VERSION 3.4)
project(project C)

set(CMAKE_C_FLAGS "-Wall -pthread")

add_executable(main main.c)