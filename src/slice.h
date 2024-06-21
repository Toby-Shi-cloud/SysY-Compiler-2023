//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_SLICE_H
#define COMPILER_SLICE_H

#include "dbg.h"
#include <cstddef>
#include <iterator>

template<typename T>
class slice {
    T *raw;
    size_t len;
public:
    using difference_type = typename std::iterator_traits<T *>::difference_type;
    using value_type = typename std::iterator_traits<T *>::value_type;
    using pointer = typename std::iterator_traits<T *>::pointer;
    using reference = typename std::iterator_traits<T *>::reference;
    using iterator_category = typename std::iterator_traits<T *>::iterator_category;

    slice(T *raw, size_t len) : raw(raw), len(len) {}

    reference operator[](size_t index) {
        assert(index < len);
        return raw[index];
    }

    size_t size() {
        return len;
    }

    bool empty() {
        return len == 0;
    }

    pointer begin() {
        return raw;
    }

    pointer end() {
        return raw + len;
    }

    slice slice_to(size_t start, size_t end) {
        assert(start <= end && end <= len);
        return slice(raw + start, end - start);
    }
};

template<typename T>
using const_slice = slice<const T>;

#endif //COMPILER_SLICE_H
