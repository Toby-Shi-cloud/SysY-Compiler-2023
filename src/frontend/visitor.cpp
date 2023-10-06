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
