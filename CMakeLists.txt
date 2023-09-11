cmake_minimum_required(VERSION 3.26)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_DEBUG_)

add_executable(Compiler
        src/main.cpp src/dbg.h
        src/frontend/lexer.cpp
        src/frontend/lexer.h)
