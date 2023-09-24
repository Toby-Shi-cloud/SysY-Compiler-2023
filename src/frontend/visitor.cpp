//
// Created by toby on 2023/9/22.
//

#include <iostream>
#include "visitor.h"

namespace frontend::visitor {
    void SysYVisitor::visitChildren(const GrammarNode &node) {
        for (const auto &ptr: node.children) {
            visit(*ptr);
        }
    }

    void SysYVisitor::visit(const GrammarNode &node) {
        if (node.type >= grammar_type::MulExp && node.type <= grammar_type::LOrExp) {
            visitExp(node);
            return;
        }
        visitChildren(node);
        if (node.type == grammar_type::Terminal) {
            // static_cast should be safe here
            out << static_cast<const TerminalNode &>(node).token << std::endl; // NOLINT
        } else {
            if (node.type != grammar_type::BlockItem && node.type != grammar_type::Decl &&
                node.type != grammar_type::BType &&
                !(node.type >= grammar_type::AssignStmt && node.type <= grammar_type::PrintfStmt))
                out << "<" << node.type << ">" << std::endl;
        }
    }

    void SysYVisitor::visitExp(const frontend::grammar::GrammarNode &node) {
        for (const auto &child : node.children) {
            visit(*child);
            if (child->type != grammar_type::Terminal) {
                out << "<" << node.type << ">" << std::endl;
            }
        }
    }
}
