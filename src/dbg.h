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
#endif // _DEBUG_

#include <fstream>
#include <cassert>
#include <iostream>

template<typename...Args>
struct overloaded : Args... {
    using Args::operator()...;
};

template<typename...Args>
overloaded(Args...) -> overloaded<Args...>;

#define TODO(msg) (std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << "TODO: " << (msg) << std::endl, exit(-1))

#endif //COMPILER_DBG_H
