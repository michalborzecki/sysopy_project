cmake_minimum_required(VERSION 3.4)

set(CMAKE_C_FLAGS "-Wall")

add_executable(cp_main main.c)
add_executable(cp_producer producer.c)
add_executable(cp_consumer consumer.c)
