//
// Created by toby on 2023/9/16.
//

#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "grammar.h"
#include <unordered_map>

namespace frontend::visitor {
    using namespace frontend::grammar;
}

#include "../mir/value.h"
#include <deque>

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
        store_type_t insert(std::string_view name, mir::pType type, bool isConstant = false);

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
        using return_type = void;

        message_queue_t &message_queue;

        /**
         * Visit all children of the node. <br>
         * Use this method only if you have nothing to do with the node itself.
         */
        return_type visitChildren(const GrammarNode &node);

        /**
         * The series of methods actually do the work. <br>
         * They are called by visit (overload method which has no template parameters)
         * according to the type of the node. <br>
         * By default, the methods do nothing but call visitChildren. <br>
         * To let the methods do something, specialize them.
         */
        template<grammar_type_t type>
        return_type visit(const GrammarNode &node);

    public:
        explicit SysYVisitor(message_queue_t &message_queue) : message_queue(message_queue) {}

        /**
         * Visit the node. <br>
         * This method is exposed to the user. <br>
         * This method will no nothing but call the corresponding method according
         * to the type of the node.
         */
        return_type visit(const GrammarNode &node);
    };
}

#endif //COMPILER_VISITOR_H
