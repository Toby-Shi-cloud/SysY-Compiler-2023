cmake_minimum_required(VERSION 3.10)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG_)
endif ()

if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    include_directories(/usr/local/include)
endif ()

file(GLOB_RECURSE SRC_DIR "src/*.cpp")
set_property(SOURCE ${SRC_DIR} PROPERTY COMPILE_FLAGS "-Werror -g")
add_executable(Compiler ${SRC_DIR})

if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    enable_testing()
    add_test(NAME LLVM-IR-Test-1
            COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d C1 -d B1 -d A1
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-2
            COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d C2 -d B2 -d A2
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-3
            COMMAND ./auto_test.py llvm -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d D1 -d D2
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-1
            COMMAND ./auto_test.py mips -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d C1 -d B1 -d A1
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-2
            COMMAND ./auto_test.py mips -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d C2 -d B2 -d A2
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-3
            COMMAND ./auto_test.py mips -s ${CMAKE_CURRENT_BINARY_DIR}/Compiler -d D1 -d D2
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set_tests_properties(LLVM-IR-Test-1 LLVM-IR-Test-2 LLVM-IR-Test-3
            PROPERTIES TIMEOUT 60)
    set_tests_properties(MIPS-Test-1 MIPS-Test-2 MIPS-Test-3
            PROPERTIES TIMEOUT 120)
endif ()
