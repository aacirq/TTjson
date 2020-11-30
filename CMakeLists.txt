cmake_minimum_required(VERSION 3.17)
project(leptjson C)

set(CMAKE_C_STANDARD 99)

add_executable(leptjson
        test.c
        leptjson.c
        leptjson.h)
