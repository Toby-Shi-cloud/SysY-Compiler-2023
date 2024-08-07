cmake_minimum_required(VERSION 3.10)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-D_DEBUG_)
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE SRC_DIR "src/*.cpp")
set_property(SOURCE ${SRC_DIR} PROPERTY COMPILE_FLAGS "-Werror")
add_executable(Compiler ${SRC_DIR})

enable_testing()
add_test(NAME Functional-LLVM-O0
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O0-HIDDEN
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d h_functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O2
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-LLVM-O2-HIDDEN
        COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d h_functional -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-ASM-O0
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-ASM-O0-HIDDEN
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d h_functional -O0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-ASM-O2
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d functional -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-ASM-O2-HIDDEN
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d h_functional -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-PERF
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d performance -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME Functional-FINAL
        COMMAND ./auto_test.py asm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d h_performance -O2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
