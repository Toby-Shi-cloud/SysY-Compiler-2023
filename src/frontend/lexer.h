//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <ostream>
#include "token.h"

namespace frontend::lexer {
    using namespace frontend::token;

    class Lexer {
        using position_t = std::string_view::iterator;
        std::string_view source;
        std::string_view current;
        size_t _line;
        position_t _last_newline;

        class Iterator {
            Lexer *self;
            token_opt current;
        public:
            inline explicit Iterator(Lexer *self, token_opt token) : self(self), current(token) {}

            inline Token operator*() const { return current.value(); }

            inline bool operator==(const Iterator &other) const {
                return self == other.self && !current && !other.current;
            }

            inline bool operator!=(const Iterator &other) const { return !(*this == other); }

            inline Iterator &operator++() {
                current = self->next_token();
                return *this;
            }
        };

    public:
        explicit Lexer(std::string_view source) : source(source), current(source), _line(1),
                                                  _last_newline(source.begin() - 1) {}

        [[nodiscard]] size_t line() const { return _line; }

        [[nodiscard]] size_t column() const { return current.begin() - _last_newline; }

        [[nodiscard]] Iterator begin() { return Iterator(this, next_token()); }

        [[nodiscard]] Iterator end() { return Iterator(this, std::nullopt); }

        inline token_opt next_token() { return next_token_impl(); }

    private:
        token_opt next_token_impl();

        inline void next_token_skip_whitespaces();

        inline bool next_token_skip_comment();

        inline token_opt next_token_try_word();

        inline token_opt next_token_try_number();

        inline token_opt next_token_try_string();

        inline token_opt next_token_try_identifier();

        inline Token next_token_error_token();
    };
}

inline std::ostream &operator<<(std::ostream &os, const frontend::lexer::Token &token) {
    return os << token.type << " " << token.raw;
}

#include "../dbg.h"

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::lexer::Token &value) {
        stream << "{ "
               << "type: " << value.type << ", "
               << "raw: " << value.raw << ", "
               << "line: " << value.line << ", "
               << "column: " << value.column
               << " }";
        return true;
    }
}
#endif // DBG_ENABLE

#endif //COMPILER_LEXER_H
