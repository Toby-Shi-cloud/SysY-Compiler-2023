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
        src/settings.h
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
        src/backend/backend_opt.cpp
        src/backend/backend_opt.h
        src/backend/reg_alloca.cpp
        src/backend/reg_alloca.h
        src/backend/translator.cpp
        src/backend/translator.h
        src/opt/constant.cpp
        src/opt/functional.cpp
        src/opt/gcm.cpp
        src/opt/lvn.cpp
        src/opt/mem2reg.cpp
        src/opt/mem2reg.h
        src/opt/opt.cpp
        src/opt/opt.h
)

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
