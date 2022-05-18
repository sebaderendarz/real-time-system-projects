cmake_minimum_required(VERSION 3.21)
project(project_1 C)

set(CMAKE_C_STANDARD 23)
SET(CMAKE_C_FLAGS -pthread)

add_executable(project_1 main.c functions.h functions.c)
