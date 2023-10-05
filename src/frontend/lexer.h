//
// Created by toby on 2023/9/11.
//

#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <ostream>
#include <optional>
#include <string_view>

using namespace std::string_view_literals;

namespace frontend::lexer::token_type {
    enum _token_type {
        ERRORTOKEN,
        INTCON, STRCON,
        MAINTK, CONSTTK, INTTK, VOIDTK, BREAKTK, CONTINUETK,
        IFTK, ELSETK, FORTK, GETINTTK, PRINTFTK, RETURNTK,
        NOT, AND, OR, PLUS, MINU, MULT, DIV, MOD,
        LSS, LEQ, GRE, GEQ, EQL, NEQ, ASSIGN, SEMICN, COMMA,
        LPARENT, RPARENT, LBRACK, RBRACK, LBRACE, RBRACE,
        IDENFR
    };

    constexpr const char *name[] = {
            "ERRORTOKEN",
            "INTCON", "STRCON",
            "MAINTK", "CONSTTK", "INTTK", "VOIDTK", "BREAKTK", "CONTINUETK",
            "IFTK", "ELSETK", "FORTK", "GETINTTK", "PRINTFTK", "RETURNTK",
            "NOT", "AND", "OR", "PLUS", "MINU", "MULT", "DIV", "MOD",
            "LSS", "LEQ", "GRE", "GEQ", "EQL", "NEQ", "ASSIGN", "SEMICN", "COMMA",
            "LPARENT", "RPARENT", "LBRACK", "RBRACK", "LBRACE", "RBRACE",
            "IDENFR"
    };

    constexpr const char *raw[] = {
            "<ERROR>",
            "<int>", "<const char *>",
            "main", "const", "int", "void", "break", "continue",
            "if", "else", "for", "getint", "printf", "return",
            "!", "&&", "||", "+", "-", "*", "/", "%",
            "<", "<=", ">", ">=", "==", "!=", "=", ";", ",",
            "(", ")", "[", "]", "{", "}",
            "<identifier>"
    };

    constexpr std::pair<std::string_view, _token_type> words[] = {
            {"main"sv,     MAINTK},
            {"const"sv,    CONSTTK},
            {"int"sv,      INTTK},
            {"void"sv,     VOIDTK},
            {"break"sv,    BREAKTK},
            {"continue"sv, CONTINUETK},
            {"if"sv,       IFTK},
            {"else"sv,     ELSETK},
            {"for"sv,      FORTK},
            {"getint"sv,   GETINTTK},
            {"printf"sv,   PRINTFTK},
            {"return"sv,   RETURNTK},
            {"&&"sv,       AND},
            {"||"sv,       OR},
            {"<="sv,       LEQ},
            {">="sv,       GEQ},
            {"=="sv,       EQL},
            {"!="sv,       NEQ},
            {"!"sv,        NOT},
            {"+"sv,        PLUS},
            {"-"sv,        MINU},
            {"*"sv,        MULT},
            {"/"sv,        DIV},
            {"%"sv,        MOD},
            {"<"sv,        LSS},
            {">"sv,        GRE},
            {"="sv,        ASSIGN},
            {";"sv,        SEMICN},
            {","sv,        COMMA},
            {"("sv,        LPARENT},
            {")"sv,        RPARENT},
            {"["sv,        LBRACK},
            {"]"sv,        RBRACK},
            {"{"sv,        LBRACE},
            {"}"sv,        RBRACE}
    };
}

namespace frontend::lexer {
    struct Token;
    using token_type_t = token_type::_token_type;
    using token_opt = std::optional<Token>;

    struct Token {
        token_type_t type;
        std::string_view raw;
        size_t line;
        size_t column;
    };

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

inline std::ostream &operator<<(std::ostream &os, frontend::lexer::token_type_t type) {
    return os << frontend::lexer::token_type::name[type];
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
