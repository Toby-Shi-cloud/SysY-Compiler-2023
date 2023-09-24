//
// Created by toby on 2023/9/16.
//

#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "grammar.h"

namespace frontend::visitor {
    using namespace frontend::grammar;

    class SysYVisitor {
        std::ostream &out;

    public:
        explicit SysYVisitor(std::ostream &out = std::cout) : out(out) {}

        void visitChildren(const GrammarNode &node);

        void visit(const GrammarNode &node);

        void visitExp(const GrammarNode &node);
    };
}

#endif //COMPILER_VISITOR_H
