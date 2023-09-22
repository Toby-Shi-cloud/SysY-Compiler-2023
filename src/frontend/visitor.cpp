//
// Created by toby on 2023/9/22.
//

#include "visitor.h"

namespace frontend::visitor {
    void SysYVisitor::visitChildren(const GrammarNode &node) {
        for (const auto &ptr : node.children) {
            visit(*ptr);
        }
    }

    void SysYVisitor::visit(const GrammarNode &node) {
        visitChildren(node);
        if (node.type == grammar_type::Terminal) {
            // static_cast should be safe here
            out << static_cast<const TerminalNode &>(node).token << std::endl; // NOLINT
        } else {
            out << "<" << node.type << ">" << std::endl;
        }
    }
}
