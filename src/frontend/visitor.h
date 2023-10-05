//
// Created by toby on 2023/9/16.
//

#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "grammar.h"

namespace frontend::visitor {
    using namespace frontend::grammar;
}

#include "../mir/value.h"

namespace frontend::visitor {
    class SymbolTable {
        using store_type_t = const mir::Value *;
        using table_t = std::unordered_map<std::string_view, store_type_t>;

        /**
         * Use std::deque to iterate from the top to the bottom. <br>
         * Stack should never be empty.
         */
        std::deque<table_t> stack;
        bool _cached = false;

    public:
        explicit SymbolTable() { stack.emplace_back(); }

        inline void enter_cache_block() { _cached ? (0) : (_cached = true, stack.emplace_back(), 0); }

        inline void enter_block() { _cached ? (_cached = false, 0) : (stack.emplace_back(), 0); }

        inline void exit_block() { stack.pop_back(); }

        /**
         * @brief Insert a new symbol into the current block.
         * @return the inserted symbol or nullptr if the symbol already exists.
         */
        store_type_t insert(std::string_view name, mir::pType type);

        /**
         * @brief Lookup a symbol in the current block.
         * @return the symbol or nullptr if the symbol does not exist.
         */
        store_type_t lookup(std::string_view name);
    };
}

#include "message.h"

namespace frontend::visitor {
    class SysYVisitor {
        message_queue_t &message_queue;

    public:
        explicit SysYVisitor(message_queue_t &message_queue) : message_queue(message_queue) {}

        void visitChildren(const GrammarNode &node);

        void visit(const GrammarNode &node);
    };
}

#endif //COMPILER_VISITOR_H
