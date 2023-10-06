//
// Created by toby on 2023/9/22.
//

#include <iostream>
#include "visitor.h"

namespace frontend::visitor {
    SymbolTable::store_type_t SymbolTable::insert(std::string_view name, mir::pType type, bool isConstant) {
        /** TODO:
         * <li> To create some real instructions later. </li>
         * <li> Values are owned by BasicBlocks, but now we still not have any,
         * which means there is a memory leak here (Values are never freed and
         * when the block is popped no one can find them). We can fix it by using
         * unique_ptr or deleting them manually, but after all this is a temporary
         * code.</li>
         */
        if (stack.back().count(name)) return nullptr;
        auto value = new mir::Value(type, isConstant);
        stack.back()[name] = value;
        return value;
    }

    SymbolTable::store_type_t SymbolTable::lookup(std::string_view name) {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (it->count(name)) return it->at(name);
        }
        return nullptr;
    }
}

// visitor: helper functions
namespace frontend::visitor {
    template<grammar_type_t type>
    bool is_specific_type(const pcGrammarNode &node) {
        return node->type == type;
    }

    inline std::optional<message> check_valid(const lexer::Token &str_token) {
        assert(str_token.type == lexer::token_type::STRCON);
        using namespace std::string_literals;
        bool rslash = false;
        bool percent = false;
        for (const char &ch: str_token.raw.substr(1, str_token.raw.size() - 2)) {
            auto line = str_token.line;
            auto column = str_token.column + (&ch - str_token.raw.data());
            if (rslash) {
                if (ch != 'n')
                    return message{message::ERROR, 'a', line, column - 1, "invalid escape sequence '\\"s + ch + "'"};
                rslash = false;
            } else if (percent) {
                if (ch != 'd')
                    return message{message::ERROR, 'a', line, column - 1, "invalid format specifier '%"s + ch + "'"};
                percent = false;
            } else {
                if (ch == '\\') {
                    rslash = true;
                } else if (ch == '%') {
                    percent = true;
                } else if (ch != 32 && ch != 33 && ch < 40 && ch > 126) {
                    return message{message::ERROR, 'a', line, column, "invalid character '"s + ch + "'"};
                }
            }
        }
        if (rslash)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2, "invalid escape sequence '\\'"};
        if (percent)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2, "invalid format specifier '%'"};
        return std::nullopt;
    }
}

// visitor: specific methods
namespace frontend::visitor {
    using namespace grammar_type;

    template<>
    SysYVisitor::return_type_t SysYVisitor::visit<PrintfStmt>(const GrammarNode &node) {
        // PRINTFTK LPARENT STRCON (COMMA exp)* RPARENT SEMICN
        assert(node.children[2]->type == Terminal);
        auto &strcon = static_cast<const TerminalNode &>(*node.children[2]).token; // NOLINT
        if (auto msg = check_valid(strcon)) {
            message_queue.push_back(*msg);
        }
        auto count_params = std::count_if(node.children.begin(), node.children.end(), &is_specific_type<Exp>);
        auto count_format = std::count(strcon.raw.begin(), strcon.raw.end(), '%');
        if (count_format != count_params) {
            using namespace std::string_literals;
            std::string msg =
                    "too "s + (count_params < count_format ? "few" : "many") +
                    " arguments for format " + std::string(strcon.raw) +
                    ", expected " + std::to_string(count_format) +
                    " but " + std::to_string(count_params) + " given";
            auto &printftk = static_cast<const TerminalNode &>(*node.children[0]).token; // NOLINT
            message_queue.push_back(message{message::ERROR, 'l', printftk.line, printftk.column, msg});
        }
        visitChildren(node);
    }
}

// visitor: default methods
namespace frontend::visitor {
    SysYVisitor::return_type_t SysYVisitor::visit(const GrammarNode &node) {
        using namespace grammar_type;
        static decltype(&SysYVisitor::visit<CompUnit>) methods[] =
                {&SysYVisitor::visit<CompUnit>, &SysYVisitor::visit<Decl>, &SysYVisitor::visit<ConstDecl>,
                 &SysYVisitor::visit<BType>, &SysYVisitor::visit<ConstDef>, &SysYVisitor::visit<ConstInitVal>,
                 &SysYVisitor::visit<VarDecl>, &SysYVisitor::visit<VarDef>, &SysYVisitor::visit<InitVal>,
                 &SysYVisitor::visit<FuncDef>, &SysYVisitor::visit<MainFuncDef>, &SysYVisitor::visit<FuncType>,
                 &SysYVisitor::visit<FuncFParams>, &SysYVisitor::visit<FuncFParam>, &SysYVisitor::visit<Block>,
                 &SysYVisitor::visit<BlockItem>, &SysYVisitor::visit<Stmt>, &SysYVisitor::visit<AssignStmt>,
                 &SysYVisitor::visit<ExpStmt>, &SysYVisitor::visit<BlockStmt>, &SysYVisitor::visit<IfStmt>,
                 &SysYVisitor::visit<ForLoopStmt>, &SysYVisitor::visit<BreakStmt>, &SysYVisitor::visit<ContinueStmt>,
                 &SysYVisitor::visit<ReturnStmt>, &SysYVisitor::visit<GetintStmt>, &SysYVisitor::visit<PrintfStmt>,
                 &SysYVisitor::visit<ForStmt>, &SysYVisitor::visit<Exp>, &SysYVisitor::visit<Cond>,
                 &SysYVisitor::visit<LVal>, &SysYVisitor::visit<PrimaryExp>, &SysYVisitor::visit<Number>,
                 &SysYVisitor::visit<UnaryExp>, &SysYVisitor::visit<UnaryOp>, &SysYVisitor::visit<FuncRParams>,
                 &SysYVisitor::visit<MulExp>, &SysYVisitor::visit<AddExp>, &SysYVisitor::visit<RelExp>,
                 &SysYVisitor::visit<EqExp>, &SysYVisitor::visit<LAndExp>, &SysYVisitor::visit<LOrExp>,
                 &SysYVisitor::visit<ConstExp>, &SysYVisitor::visit<Terminal>};
        return (this->*methods[node.type])(node);
    }

    SysYVisitor::return_type_t SysYVisitor::visitChildren(const GrammarNode &node) {
        for (const auto &ptr: node.children) {
            visit(*ptr);
        }
    }

    // default: do nothing
    template<grammar_type_t type>
    SysYVisitor::return_type_t SysYVisitor::visit(const GrammarNode &node) {
        visitChildren(node);
    }
}
