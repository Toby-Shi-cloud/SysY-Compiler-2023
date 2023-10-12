//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MESSAGE_H
#define COMPILER_MESSAGE_H

#include <vector>
#include <ostream>
#include <algorithm>

namespace frontend {
    struct message {
        enum type_t {
            INFO, WARNING, ERROR
        } type;
        int code;
        size_t line;
        size_t column;
        std::string msg;
    };

    using message_queue_t = std::vector<message>;

    inline void sort_by_line(message_queue_t &queue) {
        std::sort(queue.begin(), queue.end(), [](const message &a, const message &b) {
            return a.line < b.line;
        });
    }
}

inline std::ostream &operator<<(std::ostream &_o, frontend::message::type_t type) {
    switch (type) {
        case frontend::message::type_t::INFO:
            return _o << "INFO";
        case frontend::message::type_t::WARNING:
            return _o << "WARNING";
        case frontend::message::type_t::ERROR:
            return _o << "ERROR";
        default:
            return _o << "UNKNOWN";
    }
}

inline std::ostream &operator<<(std::ostream &_o, const frontend::message &msg) {
    return _o << msg.type << "[" << msg.line << ":" << msg.column << "]: " << msg.msg;
}

#endif //COMPILER_MESSAGE_H
