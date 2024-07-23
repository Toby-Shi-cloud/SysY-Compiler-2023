//
// Created by toby on 2023/9/11.
//

#include "frontend/lexer.h"
#include <algorithm>
#include <unordered_map>

namespace frontend::lexer {
token_opt Lexer::next_token_impl() {
    while (next_token_skip_comment()) {
    }
    next_token_skip_whitespaces();
    if (current.empty()) return std::nullopt;
    if (auto _ret = next_token_try_operator()) return _ret;
    if (auto _ret = next_token_try_number()) return _ret;
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
        current = current.substr(2);
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

const static auto operators = []() {
    constexpr auto _start = token_type::NOT;
    constexpr auto _end = token_type::IDENFR;
    std::array<std::pair<std::string_view, token_type_t>, _end - _start> arr;
    for (int it = _start; it != _end; ++it) {
        arr[it - _start].first = token_type::raw[it];
        arr[it - _start].second = static_cast<token_type_t>(it);
    }
    std::sort(arr.begin(), arr.end(), [](auto x, auto y) { return x.first.length() > y.first.length(); });
    return arr;
}();

token_opt Lexer::next_token_try_operator() {
    auto it = std::find_if(operators.cbegin(), operators.cend(),
                           [this](auto v) { return v.first == current.substr(0, v.first.length()); });
    if (it == operators.cend()) return std::nullopt;
    auto _column = column();
    current = current.substr(it->first.length());
    return Token{it->second, it->first, _line, _column};
}

token_opt Lexer::next_token_try_number() {
    if (!std::isdigit(current[0]) && current[0] != '.') return std::nullopt;
    const auto start = current;
    const auto _column = column();

    bool hex = false;
    if (current[0] == '0' && std::tolower(current[1]) == 'x') {
        hex = true;
        current = current.substr(2);
    }

    auto _len1 = std::find_if(current.cbegin(), current.cend(), [hex](char ch) {
        return !(::isdigit(ch) || ch == '.' || hex && ::tolower(ch) >= 'a' && ::tolower(ch) <= 'f');
    });
    auto _part1 = start.substr(0, _len1 - start.cbegin());

    current = current.substr(_len1 - current.cbegin());
    if (_part1.find('.') == std::string_view::npos && std::tolower(current[0]) != 'e' &&
        std::tolower(current[0]) != 'p') {
        return Token{token_type::INTCON, _part1, _line, _column};
    }

    auto _len2 = current.cbegin();
    if (std::tolower(current[0]) == 'e' || std::tolower(current[0]) == 'p') {
        current = current.substr(1);
        if (current[0] == '+' || current[0] == '-') current = current.substr(1);
        _len2 = std::find_if_not(current.cbegin(), current.cend(), ::isdigit);
    }
    current = current.substr(_len2 - current.cbegin());
    return Token{token_type::FLOATCON, start.substr(0, _len2 - start.cbegin()), _line, _column};
}

const static auto keywords = []() {
    std::unordered_map<std::string_view, token_type_t> keywords;
    for (int i = token_type::CONSTTK; i <= token_type::RETURNTK; i++) {
        keywords[token_type::raw[i]] = static_cast<token_type_t>(i);
    }
    return keywords;
}();

token_opt Lexer::next_token_try_identifier() {
    if (current[0] != '_' && !std::isalpha(current[0])) return std::nullopt;
    auto start = current;
    auto _column = column();
    while (!current.empty() && (current[0] == '_' || std::isalnum(current[0]))) current = current.substr(1);
    auto raw = start.substr(0, current.begin() - start.begin());
    if (auto ty = keywords.find(raw); ty != keywords.end()) return Token{ty->second, raw, _line, _column};
    return Token{token_type::IDENFR, raw, _line, _column};
}

Token Lexer::next_token_error_token() {
    auto start = current;
    auto _column = column();
    current = current.substr(1);
    return Token{token_type::ERRORTOKEN, start.substr(0, 1), _line, _column};
}
}  // namespace frontend::lexer
