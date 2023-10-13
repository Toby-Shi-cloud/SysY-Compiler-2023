//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_DBG_H
#define COMPILER_DBG_H

#include <cassert>

#ifdef _DEBUG_
#define DBG_ENABLE
#if __has_include(<dbg.h>) // has_include(...)
#include <dbg.h>
#elif __has_include("../lib/dbg.h")
#include "../lib/dbg.h"
#else
#undef DBG_ENABLE
#endif // has_include(...)
#endif // _DEBUG_

#ifndef DBG_ENABLE
#define dbg(...) ((void)0)
#endif

#endif //COMPILER_DBG_H
