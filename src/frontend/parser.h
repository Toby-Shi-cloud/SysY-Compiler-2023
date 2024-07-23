//
// Created by toby on 2023/9/16.
//

#ifndef COMPILER_PARSER_H
#define COMPILER_PARSER_H

#include <list>
#include <utility>
#include "grammar.h"
#include "lexer.h"
#include "message.h"

namespace frontend::parser {
class SysYParser;

using namespace frontend::grammar;
using pGrammarNodeList = std::list<pGrammarNode>;
using optGrammarNodeList = std::optional<pGrammarNodeList>;
using generator_t = std::function<optGrammarNodeList(SysYParser *)>;

enum class _option { OPTION };

enum class _many { MANY };

static constexpr _option OPTION = _option::OPTION;
static constexpr _many MANY = _many::MANY;

generator_t operator+(const generator_t &one, const generator_t &other);
generator_t operator|(const generator_t &one, const generator_t &other);
generator_t operator*(const generator_t &gen, _option);
generator_t operator*(const generator_t &gen, _many);

class SysYParser {
    using token_buffer = std::vector<token::Token>;
    using token_iterator = token_buffer::iterator;
    token_buffer tokens;
    token_iterator current;
    pGrammarNode _comp_unit;
    message_queue_t &message_queue;

    friend generator_t operator+(const generator_t &, const generator_t &);

    template <token::token_type_t type>
    static auto generator(int error_code = 0) -> generator_t {
        return [error_code](SysYParser *self) -> optGrammarNodeList {
            if (auto node = self->need_terminal(type, error_code)) {
                pGrammarNodeList result{};
                result.push_back(std::move(node));
                return result;
            }
            return std::nullopt;
        };
    }

    template <grammar_type_t type>
    static auto generator() -> generator_t {
        return [](SysYParser *self) -> optGrammarNodeList {
            if (auto node = self->parse_impl<type>()) {
                pGrammarNodeList result{};
                result.push_back(std::move(node));
                return result;
            }
            return std::nullopt;
        };
    }

    pTerminalNode need_terminal(token::token_type_t type, int error_code);

    pGrammarNode grammarNode(grammar_type_t type, const generator_t &gen) {
        if (auto list = gen(this)) {
            pGrammarNode ret{new GrammarNode(type)};
            ret->push_all(std::move(list.value()));
            return ret;
        }
        return nullptr;
    }

    template <grammar_type_t>
    pGrammarNode parse_impl();

 public:
    SysYParser(lexer::Lexer lexer, message_queue_t &message_queue) : message_queue(message_queue) {
        for (auto token : lexer) {
            tokens.push_back(token);
        }
        current = tokens.begin();
    }

    [[nodiscard]] const GrammarNode &comp_unit() const { return *_comp_unit; }

    void parse();
};
}  // namespace frontend::parser

#endif  // COMPILER_PARSER_H
