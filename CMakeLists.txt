cmake_minimum_required(VERSION 3.26)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_DEBUG_)

add_executable(Compiler main.cpp header/dbg.h)
