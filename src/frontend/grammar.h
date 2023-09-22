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

    constexpr std::string_view name[] = {
            "CompUnit"sv, "Decl"sv,
            "ConstDecl"sv, "BType"sv, "ConstDef"sv, "ConstInitVal"sv,
            "VarDecl"sv, "VarDef"sv, "InitVal"sv,
            "FuncDef"sv, "MainFuncDef"sv, "FuncType"sv, "FuncFParams"sv, "FuncFParam"sv,
            "Block"sv, "BlockItem"sv,
            "Stmt"sv, "AssignStmt"sv, "ExpStmt"sv, "BlockStmt"sv, "IfStmt"sv, "ForLoopStmt"sv,
            "BreakStmt"sv, "ContinueStmt"sv, "ReturnStmt"sv, "GetintStmt"sv, "PrintfStmt"sv,
            "ForStmt"sv, "Exp"sv, "Cond"sv, "LVal"sv, "PrimaryExp"sv, "Number"sv, "UnaryExp"sv, "UnaryOp"sv,
            "FuncRParams"sv, "MulExp"sv, "AddExp"sv, "RelExp"sv, "EqExp"sv, "LAndExp"sv, "LOrExp"sv, "ConstExp"sv,
            "Terminal"sv
    };
}

// Forward declaration
namespace frontend::grammar {
    struct GrammarNode;
    using pGrammarNode = std::unique_ptr<GrammarNode>;
    using grammar_type_t = grammar_type::_grammar_type;

    struct GrammarNode {
        const grammar_type_t type;
        std::vector<pGrammarNode> children;

        explicit GrammarNode(grammar_type_t type) : type(type) {}

        template<typename T>
        inline void push_all(T c) { for (auto &child : c) children.push_back(std::move(child)); }
    };

    struct TerminalNode : GrammarNode {
        const lexer::Token token;

        explicit TerminalNode(const lexer::Token &token) : GrammarNode(grammar_type::Terminal), token(token) {}
    };

    using pTerminalNode = std::unique_ptr<TerminalNode>;
}

inline std::ostream &operator<<(std::ostream &os, frontend::grammar::grammar_type_t type) {
    return os << frontend::grammar::grammar_type::name[type];
}

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::grammar::GrammarNode &value) {
        stream << value.type << " {";
        bool first = true;
        for (const auto &child : value.children) {
            if (first) first = false;
            else stream << ", ";
            stream << child->type;
        }
        stream << "}";
        return true;
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const frontend::grammar::TerminalNode &value) {
        stream << value.type << " ";
        pretty_print(stream, value.token);
        return true;
    }
}
#endif // DBG_ENABLE

#endif //COMPILER_GRAMMAR_H
