cmake_minimum_required(VERSION 3.0)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG_)
endif ()

add_executable(Compiler
        src/main.cpp src/dbg.h
        src/frontend/lexer.cpp
        src/frontend/lexer.h
        src/frontend/grammar.h
        src/frontend/parser.h
        src/frontend/visitor.h
        src/frontend/parser.cpp
        src/frontend/visitor.cpp
        src/frontend/message.h
        src/mir/type.cpp
        src/mir/type.h
        src/mir/value.h
)
