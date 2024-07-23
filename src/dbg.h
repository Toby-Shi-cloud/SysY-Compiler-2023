//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_DBG_H
#define COMPILER_DBG_H

#if defined(_DEBUG_)
#define DBG_ENABLE
#define DBG_MACRO_NO_WARNING
#include <dbg_macro.h>
#undef DBG_MACRO_NO_WARNING
#else
#undef DBG_ENABLE
#define dbg(...) ((void)0)
#endif  // _DEBUG_

#include <cassert>
#include <fstream>
#include <iostream>

#include "util.h"

#endif  // COMPILER_DBG_H
