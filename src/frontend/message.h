//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MESSAGE_H
#define COMPILER_MESSAGE_H

#include <algorithm>
#include <ostream>
#include <vector>
#include "../enum.h"

namespace frontend {
using magic_enum::operator<<;
struct message {
    enum type_t { INFO, WARNING, ERROR } type;

    int code;
    size_t line;
    size_t column;
    std::string msg;
};

using message_queue_t = std::vector<message>;

inline void sort_by_line(message_queue_t &queue) {
    std::sort(queue.begin(), queue.end(),
              [](const message &a, const message &b) { return a.line < b.line; });
}
}  // namespace frontend

inline std::ostream &operator<<(std::ostream &os, const frontend::message &msg) {
    return os << msg.type << "[" << msg.line << ":" << msg.column << "]: " << msg.msg;
}

#endif  // COMPILER_MESSAGE_H
