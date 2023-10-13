//
// Created by toby on 2023/9/17.
//

#ifndef COMPILER_GRAMMAR_H
#define COMPILER_GRAMMAR_H

#include <memory>
#include <vector>
#include "lexer.h"

// Grammar type
namespace frontend::grammar::grammar_type {
    enum _grammar_type {
        CompUnit, Decl,
        ConstDecl, BType, ConstDef, ConstInitVal,
        VarDecl, VarDef, InitVal,
        FuncDef, MainFuncDef, FuncType, FuncFParams, FuncFParam,
        Block, BlockItem,
        Stmt, AssignStmt, ExpStmt, BlockStmt, IfStmt, ForLoopStmt,
        BreakStmt, ContinueStmt, ReturnStmt, GetintStmt, PrintfStmt,
        ForStmt, Exp, Cond, LVal, PrimaryExp, Number, UnaryExp, UnaryOp,
        FuncRParams, MulExp, AddExp, RelExp, EqExp, LAndExp, LOrExp, ConstExp,
        Terminal
    };

    constexpr const char *name[] = {
            "CompUnit", "Decl",
            "ConstDecl", "BType", "ConstDef", "ConstInitVal",
            "VarDecl", "VarDef", "InitVal",
            "FuncDef", "MainFuncDef", "FuncType", "FuncFParams", "FuncFParam",
            "Block", "BlockItem",
            "Stmt", "AssignStmt", "ExpStmt", "BlockStmt", "IfStmt", "ForLoopStmt",
            "BreakStmt", "ContinueStmt", "ReturnStmt", "GetintStmt", "PrintfStmt",
            "ForStmt", "Exp", "Cond", "LVal", "PrimaryExp", "Number", "UnaryExp", "UnaryOp",
            "FuncRParams", "MulExp", "AddExp", "RelExp", "EqExp", "LAndExp", "LOrExp", "ConstExp",
            "Terminal"
    };
}

// Forward declaration
namespace frontend::grammar {
    struct GrammarNode;
    using pGrammarNode = std::unique_ptr<GrammarNode>;
    using pcGrammarNode = std::unique_ptr<const GrammarNode>;
    using grammar_type_t = grammar_type::_grammar_type;

    struct GrammarNode {
        const grammar_type_t type;
        std::vector<pcGrammarNode> children;

        explicit GrammarNode(grammar_type_t type) : type(type) {}

        [[nodiscard]] inline const lexer::Token &getToken() const;

        template<typename T>
        inline void push_all(T c) { for (auto &child : c) children.push_back(std::move(child)); }
    };

    struct TerminalNode : GrammarNode {
        const lexer::Token token;

        explicit TerminalNode(const lexer::Token &token) : GrammarNode(grammar_type::Terminal), token(token) {}
    };

    using pTerminalNode = std::unique_ptr<TerminalNode>;

    inline const lexer::Token &GrammarNode::getToken() const {
        assert(type == grammar_type::Terminal);
        return static_cast<const TerminalNode &>(*this).token; // NOLINT
    }
}

inline std::ostream &operator<<(std::ostream &os, frontend::grammar::grammar_type_t type) {
    return os << frontend::grammar::grammar_type::name[type];
}

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::grammar::TerminalNode &value) {
        stream << value.type << " ";
        pretty_print(stream, value.token);
        return true;
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::grammar::GrammarNode &value) {
        if (value.type == frontend::grammar::grammar_type::Terminal) {
            // NOLINTNEXTLINE
            return pretty_print(stream, static_cast<const frontend::grammar::TerminalNode &>(value));
        }
        stream << value.type << " { ";
        bool first = true;
        for (const auto &child : value.children) {
            if (first) first = false;
            else stream << ", ";
            stream << child->type;
        }
        stream << " }";
        return true;
    }
}
#endif // DBG_ENABLE

#endif //COMPILER_GRAMMAR_H
