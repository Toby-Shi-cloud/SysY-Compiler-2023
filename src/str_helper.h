//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_STR_HELPER_H
#define COMPILER_STR_HELPER_H

#include <cstring>
#include <iostream>

namespace str {
    template<typename R, typename D>
    struct join {
        const R &range;
        const D &delimiter;

        join(const R &range, const D &delimiter) : range(range), delimiter(delimiter) {}
    };

    template<typename Os, typename R, typename D>
    auto operator<<(Os &os, const join<R, D> &j) ->
    decltype(os << j.delimiter, os << *j.range.begin(), os) {
        auto it = j.range.begin();
        if (it != j.range.end()) os << *it++;
        for (; it != j.range.end(); ++it) os << j.delimiter << *it;
        return os;
    }

    struct repeat {
        std::string_view str;
        size_t times;

        repeat(std::string_view str, size_t times) : str(str), times(times) {}
    };

    inline std::ostream &operator<<(std::ostream &os, const repeat &r) {
        for (size_t i = 0; i < r.times; ++i) os << r.str;
        return os;
    }
}

#endif //COMPILER_STR_HELPER_H
