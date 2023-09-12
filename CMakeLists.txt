cmake_minimum_required(VERSION 3.26)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG_)
endif ()

add_executable(Compiler
        src/main.cpp src/dbg.h
        src/frontend/lexer.cpp
        src/frontend/lexer.h)
