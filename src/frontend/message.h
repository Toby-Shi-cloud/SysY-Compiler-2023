//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MESSAGE_H
#define COMPILER_MESSAGE_H

#include <vector>
#include <ostream>

namespace frontend {
    struct message {
        enum class type_t {
            info, warning, error
        } type;
        int code;
        int line;
        int column;
        const char *msg;
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
        case frontend::message::type_t::info:
            return _o << "INFO";
        case frontend::message::type_t::warning:
            return _o << "WARNING";
        case frontend::message::type_t::error:
            return _o << "ERROR";
    }
}

inline std::ostream &operator<<(std::ostream &_o, const frontend::message &msg) {
    return _o << msg.type << ": " << msg.msg << " at line " << msg.line << ", column " << msg.column << std::endl;
}

#endif //COMPILER_MESSAGE_H
