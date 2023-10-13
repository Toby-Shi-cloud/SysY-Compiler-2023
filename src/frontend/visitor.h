//
// Created by toby on 2023/9/16.
//

#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "message.h"
#include "grammar.h"
#include "../mir/manager.h"
#include <list>
#include <stack>
#include <unordered_map>

namespace frontend::visitor {
    using namespace frontend::grammar;
}

namespace frontend::visitor {
    class SymbolTable {
        using store_type_t = mir::Value *;
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
         * @param name the name of the symbol.
         * @param value the value of the symbol.
         * @param token the token where the symbol is defined.
         * @return a message if the symbol already exists, or nullopt if the symbol is inserted successfully.
         */
        std::optional<message> insert(std::string_view name, store_type_t value, const lexer::Token &token);

        /**
         * @brief Lookup a symbol in the current block.
         * @return the symbol or nullptr if the symbol does not exist.
         */
        store_type_t lookup(std::string_view name);
    };
}

#include "message.h"

namespace frontend::visitor {
    using GrammarIterator = std::vector<pcGrammarNode>::const_iterator;

    class SysYVisitor {
        using value_type = mir::Value *;
        using value_list = std::list<value_type>;
        using value_vector = std::vector<value_type>;
        using return_type = std::tuple<value_type, value_list>;

        struct loop_info {
            mir::BasicBlock *continue_block;
            mir::BasicBlock *break_block;
        };

        mir::Manager &manager;
        message_queue_t &message_queue;
        SymbolTable symbol_table;
        mir::Literal *zero_value;
        std::stack<loop_info> loop_stack;
        mir::Function *current_function;

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
        explicit SysYVisitor(mir::Manager &manager, message_queue_t &message_queue)
                : manager(manager), message_queue(message_queue),
                  zero_value(new mir::Literal(mir::make_literal(0))) {
            manager.literalPool.insert(zero_value);
        }

        /**
         * Visit the node. <br>
         * This method is exposed to the user. <br>
         * This method will no nothing but call the corresponding method according
         * to the type of the node.
         */
        return_type visit(const GrammarNode &node);

        /* Helper methods */
    private:
        /**
         * To get the type of a variable. <br>
         * The parameter should be like this: <br>
         *     LBRACK constExp? RBRACK (LBRACK constExp RBRACK)*
         */
        mir::pType getVarType(GrammarIterator begin, GrammarIterator end);

        /**
         * A visitor helper for binary expressions. <br>
         * Note: LogicalAnd and LogicalOr are not included,
         * because they need short-circuit evaluation.
         */
        return_type visitBinaryExp(const GrammarNode &node);

        /**
         * A visitor helper for many exps, which are split by any terminal symbol. <br>
         * Return a pair of value_vector and value_list. <br>
         */
        std::pair<value_vector, value_list>
        visitExps(GrammarIterator begin, GrammarIterator end, value_vector init_value = {});
    };
}

#endif //COMPILER_VISITOR_H
