//
// Created by toby on 2024/6/23.
//

#ifndef COMPILER_UTIL_H
#define COMPILER_UTIL_H

template <typename... Args>
struct overloaded : Args... {
    using Args::operator()...;
};

template <typename... Args>
overloaded(Args...) -> overloaded<Args...>;

#define TODO(msg) (std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << "TODO: " << (msg) << std::endl, exit(-1))

/// Some magic defines...
#define UTIL_NUM_ARGS_IMPL_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _a, _b, _c, _d, _e, _f, N, ...) N
#define UTIL_NUM_ARGS_(...) UTIL_NUM_ARGS_IMPL_(__VA_ARGS__, f, e, d, c, b, a, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define UTIL_CONCAT_IMPL_(a, b) a##b
#define UTIL_CONCAT_(a, b) UTIL_CONCAT_IMPL_(a, b)

#define UTIL_FOR_EACH_0(func) ((void)0)
#define UTIL_FOR_EACH_1(func, x) func(x)
#define UTIL_FOR_EACH_2(func, x, ...) func(x); UTIL_FOR_EACH_1(func, __VA_ARGS__)
#define UTIL_FOR_EACH_3(func, x, ...) func(x); UTIL_FOR_EACH_2(func, __VA_ARGS__)
#define UTIL_FOR_EACH_4(func, x, ...) func(x); UTIL_FOR_EACH_3(func, __VA_ARGS__)
#define UTIL_FOR_EACH_5(func, x, ...) func(x); UTIL_FOR_EACH_4(func, __VA_ARGS__)
#define UTIL_FOR_EACH_6(func, x, ...) func(x); UTIL_FOR_EACH_5(func, __VA_ARGS__)
#define UTIL_FOR_EACH_7(func, x, ...) func(x); UTIL_FOR_EACH_6(func, __VA_ARGS__)
#define UTIL_FOR_EACH_8(func, x, ...) func(x); UTIL_FOR_EACH_7(func, __VA_ARGS__)
#define UTIL_FOR_EACH_9(func, x, ...) func(x); UTIL_FOR_EACH_8(func, __VA_ARGS__)
#define UTIL_FOR_EACH_a(func, x, ...) func(x); UTIL_FOR_EACH_9(func, __VA_ARGS__)
#define UTIL_FOR_EACH_b(func, x, ...) func(x); UTIL_FOR_EACH_a(func, __VA_ARGS__)
#define UTIL_FOR_EACH_c(func, x, ...) func(x); UTIL_FOR_EACH_b(func, __VA_ARGS__)
#define UTIL_FOR_EACH_d(func, x, ...) func(x); UTIL_FOR_EACH_c(func, __VA_ARGS__)
#define UTIL_FOR_EACH_e(func, x, ...) func(x); UTIL_FOR_EACH_d(func, __VA_ARGS__)
#define UTIL_FOR_EACH_f(func, x, ...) func(x); UTIL_FOR_EACH_e(func, __VA_ARGS__)

#define FOR_EACH(func, ...) UTIL_CONCAT_(UTIL_FOR_EACH_, UTIL_NUM_ARGS_(__VA_ARGS__))(func, __VA_ARGS__)

#endif  // COMPILER_UTIL_H
