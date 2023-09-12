//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_DBG_H
#define COMPILER_DBG_H

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
namespace dbg {
template <typename T>
T&& identity(T&& t) {
  return std::forward<T>(t);
}

template <typename T, typename... U>
auto identity(T&&, U&&... u) {
  return identity(std::forward<U>(u)...);
}
} // namespace dbg
#define dbg(...) dbg::identity(__VA_ARGS__)
#endif

#endif //COMPILER_DBG_H
