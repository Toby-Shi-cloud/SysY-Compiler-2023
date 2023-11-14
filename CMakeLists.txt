cmake_minimum_required(VERSION 3.0)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG_)
endif ()

if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    include_directories(/usr/local/include)
endif ()

add_executable(Compiler
        src/dbg.h
        src/enum.h
        src/main.cpp
        src/mips.h
        src/mir.h
        src/frontend/grammar.h
        src/frontend/lexer.cpp
        src/frontend/lexer.h
        src/frontend/message.h
        src/frontend/parser.cpp
        src/frontend/parser.h
        src/frontend/token.h
        src/frontend/visitor.cpp
        src/frontend/visitor.h
        src/mir/derived_value.cpp
        src/mir/derived_value.h
        src/mir/instruction.cpp
        src/mir/instruction.h
        src/mir/manager.h
        src/mir/type.cpp
        src/mir/type.h
        src/mir/value.h
        src/mips/alias.h
        src/mips/component.h
        src/mips/instruction.h
        src/mips/mips.cpp
        src/mips/operand.h
        src/backend/reg_alloca.cpp
        src/backend/reg_alloca.h
        src/backend/translator.cpp
        src/backend/translator.h
        src/opt/mem2reg.cpp
        src/opt/mem2reg.h
        src/opt/opt.cpp
)

if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    enable_testing()
    add_test(NAME LLVM-IR-Test-A
            COMMAND ./auto_test.sh llvm ${CMAKE_CURRENT_BINARY_DIR}/Compiler A30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-B
            COMMAND ./auto_test.sh llvm ${CMAKE_CURRENT_BINARY_DIR}/Compiler B30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-C
            COMMAND ./auto_test.sh llvm ${CMAKE_CURRENT_BINARY_DIR}/Compiler C30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME LLVM-IR-Test-D
            COMMAND ./auto_test.sh llvm ${CMAKE_CURRENT_BINARY_DIR}/Compiler D6
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-A
            COMMAND ./auto_test.sh mips ${CMAKE_CURRENT_BINARY_DIR}/Compiler A30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-B
            COMMAND ./auto_test.sh mips ${CMAKE_CURRENT_BINARY_DIR}/Compiler B30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-C
            COMMAND ./auto_test.sh mips ${CMAKE_CURRENT_BINARY_DIR}/Compiler C30
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME MIPS-Test-D
            COMMAND ./auto_test.sh mips ${CMAKE_CURRENT_BINARY_DIR}/Compiler D6
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set_tests_properties(LLVM-IR-Test-A LLVM-IR-Test-B LLVM-IR-Test-C LLVM-IR-Test-D
            PROPERTIES TIMEOUT 30)
    set_tests_properties(MIPS-Test-A MIPS-Test-B MIPS-Test-C MIPS-Test-D
            PROPERTIES TIMEOUT 60)
endif ()
