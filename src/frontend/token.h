//
// Created by toby on 2023/10/31.
//

#ifndef COMPILER_TOKEN_H
#define COMPILER_TOKEN_H

#include <array>
#include <optional>
#include <algorithm>
#include "../dbg.h"
#include "../enum.h"

namespace frontend::token::token_type {
    using namespace std::string_view_literals;

    enum _token_type {
        ERRORTOKEN,
        INTCON, STRCON,
        MAINTK, CONSTTK, INTTK, VOIDTK, BREAKTK, CONTINUETK,
        IFTK, ELSETK, WHILETK, FORTK, GETINTTK, PRINTFTK, RETURNTK,
        NOT, AND, OR, PLUS, MINU, MULT, DIV, MOD,
        LSS, LEQ, GRE, GEQ, EQL, NEQ, ASSIGN, SEMICN, COMMA,
        LPARENT, RPARENT, LBRACK, RBRACK, LBRACE, RBRACE,
        IDENFR
    };

    inline std::ostream &operator<<(std::ostream &os, _token_type type) {
        return os << magic_enum::enum_to_string(type);
    }

    constexpr const char *raw[] = {
        "<ERROR>",
        "<int>", "<char []>",
        "main", "const", "int", "void", "break", "continue",
        "if", "else", "while", "for", "getint", "printf", "return",
        "!", "&&", "||", "+", "-", "*", "/", "%",
        "<", "<=", ">", ">=", "==", "!=", "=", ";", ",",
        "(", ")", "[", "]", "{", "}",
        "<identifier>"
    };

    const auto words = []() noexcept {
        std::array<std::pair<std::string_view, _token_type>, std::size(raw)> words{};
        for (size_t i = 0; i < words.size(); ++i) {
            words[i].first = raw[i];
            words[i].second = static_cast<_token_type>(i);
        }
        std::sort(words.begin(), words.end(), [](const auto &a, const auto &b) {
            return a.first.size() > b.first.size();
        });
        return words;
    }();
}

namespace frontend::token {
    struct Token;
    using token_type_t = token_type::_token_type;
    using token_opt = std::optional<Token>;

    struct Token {
        token_type_t type;
        std::string_view raw;
        size_t line;
        size_t column;
    };

    inline std::ostream &operator<<(std::ostream &os, const Token &token) {
        return os << token.type << " " << token.raw;
    }
}

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::token::Token &value) {
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

#endif //COMPILER_TOKEN_H
