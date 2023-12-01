//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_DBG_H
#define COMPILER_DBG_H

#ifdef _DEBUG_

#include <fstream>

inline std::ofstream flog1("log1.txt"), flog2("log2.txt");

#define dbg_file(file, ...) do{ \
    auto buf = std::cerr.rdbuf(file.rdbuf()); \
    dbg(__VA_ARGS__); \
    std::cerr.rdbuf(buf); \
}while(0)

#define dbg1(...) dbg_file(flog1, __VA_ARGS__)
#define dbg2(...) dbg_file(flog2, __VA_ARGS__)

#define DBG_ENABLE
#if __has_include(<dbg.h>) // has_include(...)
#include <dbg.h>
#elif __has_include("../lib/dbg.h")
#include "../lib/dbg.h"
#else
#undef DBG_ENABLE
#endif // has_include(...)
#else
#ifndef NDEBUG
#define NDEBUG
#endif
#endif // _DEBUG_

#ifndef DBG_ENABLE
#define dbg(...) ((void)0)
#define dbg1(...) ((void)0)
#define dbg2(...) ((void)0)
#endif

#include <cassert>
#include <iostream>

#endif //COMPILER_DBG_H
