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
        src/mir/derived_value.cpp
        src/mir/derived_value.h
        src/mir/instruction.cpp
        src/mir/instruction.h
        src/mir/manager.h
)

if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    enable_testing()
    add_test(NAME LLVM-IR-Test-A
            COMMAND ./auto_test.sh ${CMAKE_CURRENT_BINARY_DIR}/Compiler A30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-B
            COMMAND ./auto_test.sh ${CMAKE_CURRENT_BINARY_DIR}/Compiler B30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-C
            COMMAND ./auto_test.sh ${CMAKE_CURRENT_BINARY_DIR}/Compiler C30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-D
            COMMAND ./auto_test.sh ${CMAKE_CURRENT_BINARY_DIR}/Compiler D6
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif ()
