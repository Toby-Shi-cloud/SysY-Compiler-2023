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
    using magic_enum::operator<<;
    enum _token_type {
        ERRORTOKEN,
        INTCON, FLOATCON,
        CONSTTK, INTTK, FLOATTK, VOIDTK, BREAKTK, CONTINUETK,
        IFTK, ELSETK, WHILETK, FORTK, RETURNTK,
        NOT, AND, OR, PLUS, MINU, MULT, DIV, MOD,
        LSS, LEQ, GRE, GEQ, EQL, NEQ, ASSIGN, SEMICN, COMMA,
        LPARENT, RPARENT, LBRACK, RBRACK, LBRACE, RBRACE,
        IDENFR
    };

    constexpr const char *raw[] = {
        "<ERROR>",
        "<int>", "<float>",
        "const", "int", "float", "void", "break", "continue",
        "if", "else", "while", "for", "return",
        "!", "&&", "||", "+", "-", "*", "/", "%",
        "<", "<=", ">", ">=", "==", "!=", "=", ";", ",",
        "(", ")", "[", "]", "{", "}",
        "<identifier>"
    };
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
