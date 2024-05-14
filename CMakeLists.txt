cmake_minimum_required(VERSION 3.10)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG_)
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SRC_DIR "src/*.cpp")
set_property(SOURCE ${SRC_DIR} PROPERTY COMPILE_FLAGS "-Werror")
add_executable(Compiler ${SRC_DIR})

enable_testing()
add_test(NAME Functional-LLVM-O0
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O0-HIDDEN
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d hidden_functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O2
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O2-HIDDEN
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d hidden_functional
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
