//
// Created by toby on 2023/9/11.
//

#include "lexer.h"

namespace frontend::lexer {
    Token_opt Lexer::next_token_impl() {
        while (next_token_skip_comment());
        next_token_skip_whitespaces();
        if (current.empty()) return std::nullopt;
        if (auto _ret = next_token_try_word()) return _ret;
        if (auto _ret = next_token_try_number()) return _ret;
        if (auto _ret = next_token_try_string()) return _ret;
        if (auto _ret = next_token_try_identifier()) return _ret;
        return next_token_error_token();
    }

    void Lexer::next_token_skip_whitespaces() {
        while (!current.empty() && std::isspace(current[0])) {
            if (current[0] == '\n') {
                _line++;
                _last_newline = current.begin();
            }
            current = current.substr(1);
        }
    }

    bool Lexer::next_token_skip_comment() {
        next_token_skip_whitespaces();
        if (current.empty()) return false;
        if (current.substr(0, 2) == "//") {
            current = current.substr(current.find('\n'));
            _line++;
            _last_newline = current.begin();
            current = current.substr(1);
        } else if (current.substr(0, 2) == "/*") {
            auto end = current.find("*/");
            auto comment = current.substr(0, end);
            if (auto count = std::count(comment.begin(), comment.end(), '\n')) {
                _line += count;
                _last_newline = current.begin() + comment.rfind('\n');
            }
            current = current.substr(end + 2);
        } else {
            return false;
        }
        return true;
    }

    Token_opt Lexer::next_token_try_word() {
        for (auto &p: token_type::words) {
            if (current.substr(0, p.first.size()) == p.first) {
                auto _column = column();
                current = current.substr(p.first.size());
                return Token{p.second, p.first, _line, _column};
            }
        }
        return std::nullopt;
    }

    Token_opt Lexer::next_token_try_number() {
        if (!std::isdigit(current[0])) return std::nullopt;
        auto start = current;
        auto _column = column();
        while (!current.empty() && std::isdigit(current[0])) current = current.substr(1);
        return Token{token_type::INTCON, start.substr(0, current.begin() - start.begin()), _line, _column};
    }

    Token_opt Lexer::next_token_try_string() {
        if (current[0] != '"') return std::nullopt;
        auto end = current.substr(1).find('"');
        if (end == std::string_view::npos) return std::nullopt;
        auto _column = column();
        auto _raw = current.substr(0, end + 2);
        current = current.substr(end + 2);
        return Token{token_type::STRCON, _raw, _line, _column};
    }

    Token_opt Lexer::next_token_try_identifier() {
        if (current[0] != '_' && !std::isalpha(current[0])) return std::nullopt;
        auto start = current;
        auto _column = column();
        while (!current.empty() && (current[0] == '_' || std::isalnum(current[0]))) current = current.substr(1);
        return Token{token_type::IDENFR, start.substr(0, current.begin() - start.begin()), _line, _column};
    }

    Token Lexer::next_token_error_token() {
        auto start = current;
        auto _column = column();
        current = current.substr(1);
        return Token{token_type::ERRORTOKEN, start.substr(0, 1), _line, _column};
    }
}
