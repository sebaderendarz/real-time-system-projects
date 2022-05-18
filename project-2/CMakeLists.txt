cmake_minimum_required(VERSION 3.21)
project(cron C)

set(CMAKE_C_STANDARD 23)

add_executable(cron main.c cron.c cron.h logger.c logger.h)
target_link_libraries(cron PRIVATE pthread rt)
