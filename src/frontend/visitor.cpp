//
// Created by toby on 2023/9/22.
//

#include <iostream>
#include "visitor.h"

namespace frontend::visitor {
    SymbolTable::store_type_t SymbolTable::insert(std::string_view name, mir::pType type) {
        //TODO: to create some real instructions later.
        if (stack.back().count(name)) return nullptr;
        auto value = new mir::Value(type);
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
    void SysYVisitor::visitChildren(const GrammarNode &node) {
        for (const auto &ptr: node.children) {
            visit(*ptr);
        }
    }

    void SysYVisitor::visit(const GrammarNode &node) {
        visitChildren(node);
    }
}
