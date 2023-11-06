//
// Created by toby on 2023/9/22.
//

#include <iostream>
#include "visitor.h"

// Symbol Table
namespace frontend::visitor {
    std::optional<message> SymbolTable::insert(std::string_view name, store_type_t value, const Token &token) {
        if (stack.back().count(name)) return message{
            message::ERROR, 'b', token.line, token.column, "redefinition of '" + std::string(name) + "'"
        };
        stack.back()[name] = value;
        return std::nullopt;
    }

    SymbolTable::store_type_t SymbolTable::lookup(std::string_view name) {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (it->count(name)) return it->at(name);
        }
        return {};
    }
}

// visitor: helper functions
namespace frontend::visitor {
    template<grammar_type_t type>
    inline bool is_specific_type(const pcGrammarNode &node) {
        return node->type == type;
    }

    template<typename T>
    inline mir::Instruction *get_binary_instruction(mir::Value *lhs, mir::Value *rhs) {
        return new T{lhs, rhs};
    }

    template<mir::Instruction::icmp::Cond cond>
    inline mir::Instruction *get_icmp_instruction(mir::Value *lhs, mir::Value *rhs) {
        return new mir::Instruction::icmp{cond, lhs, rhs};
    }

    inline std::optional<message> check_valid(const Token &str_token) {
        assert(str_token.type == token_type::STRCON);
        using namespace std::string_literals;
        bool backslash = false;
        bool percent = false;
        for (const char &ch: str_token.raw.substr(1, str_token.raw.size() - 2)) {
            auto line = str_token.line;
            auto column = str_token.column + (&ch - str_token.raw.data());
            if (backslash) {
                if (ch != 'n')
                    return message{message::ERROR, 'a', line, column - 1, "invalid escape sequence '\\"s + ch + "'"};
                backslash = false;
            } else if (percent) {
                if (ch != 'd')
                    return message{message::ERROR, 'a', line, column - 1, "invalid format specifier '%"s + ch + "'"};
                percent = false;
            } else {
                if (ch == '\\') {
                    backslash = true;
                } else if (ch == '%') {
                    percent = true;
                } else if (ch != 32 && ch != 33 && (ch < 40 || ch > 126)) {
                    return message{message::ERROR, 'a', line, column, "invalid character '"s + ch + "'"};
                }
            }
        }
        if (backslash)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2,
                           "invalid escape sequence '\\'"};
        if (percent)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2,
                           "invalid format specifier '%'"};
        return std::nullopt;
    }
}

// visitor: helper methods
namespace frontend::visitor {
    using namespace grammar_type;
    using namespace token_type;
    using mir::Instruction;

    mir::pType SysYVisitor::getVarType(GrammarIterator begin, GrammarIterator end) {
        mir::pType result = mir::Type::getI32Type();

        for (auto it = std::reverse_iterator(end); it != std::reverse_iterator(begin); ++it) {
            auto &node = *it;
            if (node->type != grammar_type::Terminal) continue;
            auto &token = node->getToken();
            if (token.type == RBRACK) continue;
            assert(token.type == LBRACK);
            auto &exp = *(it - 1);
            if (exp->type == grammar_type::Terminal) {
                result = mir::PointerType::getPointerType(result);
            } else {
                assert(exp->type == grammar_type::ConstExp);
                auto [value, list] = visit(*exp);
                auto literal = dynamic_cast<mir::IntegerLiteral *>(value);
                assert(literal && list.empty());
                result = mir::ArrayType::getArrayType(literal->value, result);
            }
        }

        return result;
    }

    SysYVisitor::return_type SysYVisitor::visitBinaryExp(const GrammarNode &node) {
        using literal_operator = decltype(&mir::operator+);
        using normal_operator = decltype(&get_binary_instruction<Instruction::add>);
        using icmp = mir::Instruction::icmp;
        static std::unordered_map<token_type_t, std::pair<literal_operator, normal_operator>> call_table = {
                {PLUS, {&mir::operator+, &get_binary_instruction<Instruction::add>}},
                {MINU, {&mir::operator-, &get_binary_instruction<Instruction::sub>}},
                {MULT, {&mir::operator*, &get_binary_instruction<Instruction::mul>}},
                {DIV,  {&mir::operator/, &get_binary_instruction<Instruction::sdiv>}},
                {MOD,  {&mir::operator%, &get_binary_instruction<Instruction::srem>}},
                {LSS,  {nullptr,         &get_icmp_instruction<icmp::SLT>}},
                {LEQ,  {nullptr,         &get_icmp_instruction<icmp::SLE>}},
                {GRE,  {nullptr,         &get_icmp_instruction<icmp::SGT>}},
                {GEQ,  {nullptr,         &get_icmp_instruction<icmp::SGE>}},
                {EQL,  {nullptr,         &get_icmp_instruction<icmp::EQ>}},
                {NEQ,  {nullptr,         &get_icmp_instruction<icmp::NE>}},
        };

        auto [ret_val, ret_list] = visit(*node.children[0]);
        for (auto it = node.children.begin() + 1; it != node.children.end(); it += 2) {
            auto &token = (*it)->getToken();
            auto [value, list] = visit(*it[1]);
            ret_list.splice(ret_list.end(), list);

            auto ret_literal = dynamic_cast<mir::IntegerLiteral *>(ret_val);
            auto literal = dynamic_cast<mir::IntegerLiteral *>(value);

            auto [f, g] = call_table[token.type];

            if (f && ret_literal && literal && ret_list.empty()) {
                ret_literal = manager.getIntegerLiteral(f(*ret_literal, *literal).value);
                ret_val = ret_literal;
            } else {
                if (ret_val->getType() != mir::Type::getI32Type()) {
                    ret_val = new mir::Instruction::zext{mir::Type::getI32Type(), ret_val};
                    ret_list.push_back(ret_val);
                }
                if (value->getType() != mir::Type::getI32Type()) {
                    value = new mir::Instruction::zext{mir::Type::getI32Type(), value};
                    ret_list.push_back(value);
                }
                ret_val = g(ret_val, value);
                ret_list.push_back(ret_val);
            }
        }
        return {ret_val, ret_list};
    }

    std::pair<SysYVisitor::value_vector, SysYVisitor::value_list>
    SysYVisitor::visitExps(GrammarIterator begin, GrammarIterator end, value_vector init_value) {
        value_vector &indices = init_value;
        value_list list = {};
        std::transform(begin, end, std::back_inserter(indices),
                       [this, &list](const pcGrammarNode &ptr) -> value_type {
                           if (ptr->type == grammar_type::Terminal) return nullptr;
                           auto [value, l] = visit(*ptr);
                           list.splice(list.end(), l);
                           return value;
                       });
        indices.erase(std::remove_if(indices.begin(), indices.end(), [](value_type ptr) { return ptr == nullptr; }),
                      indices.end());
        return {indices, list};
    }

    SysYVisitor::value_list
    SysYVisitor::storeInitValue(value_type var, mir::pType type,
                                std::vector<value_type>::iterator initVal,
                                std::vector<value_type> *index) {
        static value_vector _idx = std::vector<value_type>{zero_value};
        if (index == nullptr && type == mir::Type::getI32Type()) {
            auto store = new Instruction::store(*initVal, var);
            return {store};
        } else if (type == mir::Type::getI32Type()) {
            auto ptr = new Instruction::getelementptr(type, var, *index);
            auto store = new Instruction::store(*initVal, ptr);
            return {ptr, store};
        }
        index = index ? index : &_idx;
        auto base = type->getArrayBase();
        auto ele_size = base->ssize() / 4;
        value_list ret = {};
        for (int i = 0; i < type->getArraySize(); i++) {
            auto l = manager.getIntegerLiteral(i);
            index->push_back(l);
            auto list = storeInitValue(var, base, initVal + i * ele_size, index);
            ret.splice(ret.end(), list);
            index->pop_back();
        }
        return ret;
    }

    std::pair<SysYVisitor::value_vector, SysYVisitor::value_list>
    SysYVisitor::visitInitVal(const GrammarNode &node) {
        // exp | LBRACE initVal (COMMA initVal)* RBRACE
        if (node.children.size() == 1) {
            auto [value, list] = visit(*node.children[0]);
            return {{value}, list};
        }
        value_vector values;
        value_list lists;
        for (auto &child: node.children) {
            if (child->type == Terminal) continue;
            auto [value, list] = visitInitVal(*child);
            values.insert(values.end(), value.begin(), value.end());
            lists.splice(lists.end(), list);
        }
        return {values, lists};
    }

    void SysYVisitor::listToBB(value_list &list, const Token &end_token) {
        value_list _allocas{};
        for (auto it = list.begin(); it != list.end();) {
            auto alloca_ = dynamic_cast<mir::Instruction::alloca_ *>(*it);
            if (alloca_) {
                _allocas.push_back(alloca_);
                it = list.erase(it);
            } else ++it;
        }
        _allocas.splice(_allocas.end(), list);
        list = _allocas;

        bool is_terminator = true;
        mir::BasicBlock *cur = nullptr;
        for (auto item: list) {
            auto bb = dynamic_cast<mir::BasicBlock *>(item);
            if (bb) {
                if (!is_terminator) {
                    auto br = new Instruction::br(bb);
                    cur->push_back(br);
                }
                cur = bb;
                current_function->bbs.push_back(bb);
                is_terminator = false;
            } else {
                if (is_terminator) {
                    bb = new mir::BasicBlock();
                    cur = bb;
                    current_function->bbs.push_back(bb);
                    is_terminator = false;
                }
                auto instr = dynamic_cast<mir::Instruction *>(item);
                assert(instr);
                cur->push_back(instr);
                is_terminator = instr->isTerminator();
            }
        }

        if (current_function->getType()->getFunctionRet() != mir::Type::getVoidType()) {
            if (cur == nullptr || cur->instructions.empty() || cur->instructions.back()->instrTy != Instruction::RET) {
                message_queue.push_back({
                    message::ERROR, 'g', end_token.line, end_token.column,
                    "Non-void function does not return a value"
                });
            }
        } else {
            if (cur == nullptr) {
                cur = new mir::BasicBlock();
                current_function->bbs.push_back(cur);
                cur->push_back(new Instruction::ret());
            } else if (cur->instructions.empty() || !cur->instructions.back()->isTerminator()) {
                cur->push_back(new Instruction::ret());
            }
        }
    }

    SysYVisitor::value_type SysYVisitor::truncToI1(value_type value, value_list &list) {
        if (value->getType() == mir::Type::getI1Type()) return value;
        auto icmp = new Instruction::icmp(mir::Instruction::icmp::NE, value, zero_value);
        list.push_back(icmp);
        return icmp;
    }
}

// visitor: specific methods
namespace frontend::visitor {
    template<>
    SysYVisitor::return_type SysYVisitor::visit<ConstDef>(const GrammarNode &node) {
        // IDENFR (LBRACK constExp RBRACK)* ASSIGN constInitVal
        auto &identifier = node.children[0]->getToken();
        auto type = getVarType(node.children.begin() + 1, node.children.end() - 2);
        auto [value, list] = visit(*node.children.back());
        auto literal = dynamic_cast<mir::Literal *>(value);
        assert(literal && list.empty());
        // always global
        mir::GlobalVar *variable;
        if (current_function) {
            variable = new mir::GlobalVar(type, literal, true);
            variable->setName("@__const." + current_function->getName().substr(1) + "." + std::string(identifier.raw));
        } else {
            variable = new mir::GlobalVar(type, std::string(identifier.raw), literal, true);
        }
        manager.globalVars.push_back(variable);
        if (auto msg = symbol_table.insert(identifier.raw, {variable, literal}, identifier)) {
            message_queue.push_back(*msg);
        }
        return {};
    }

    template<> SysYVisitor::return_type SysYVisitor::visit<ConstExp>(const GrammarNode &node);

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ConstInitVal>(const GrammarNode &node) {
        // constExp | LBRACE constInitVal (COMMA constInitVal)* RBRACE
        if (node.children[0]->type != Terminal) return visit<ConstExp>(*node.children[0]);
        std::vector<mir::Literal *> literals;
        for (auto &child: node.children) {
            if (child->type == Terminal) continue;
            auto [value, list] = visit<ConstInitVal>(*child);
            auto literal = dynamic_cast<mir::Literal *>(value);
            assert(literal && list.empty());
            literals.push_back(literal);
        }
        return {new mir::ArrayLiteral(std::move(literals)), {}};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<VarDef>(const GrammarNode &node) {
        // IDENFR (LBRACK constExp RBRACK)* (ASSIGN initVal)?
        auto &identifier = node.children[0]->getToken();
        auto &initVal = node.children.back()->type == InitVal ? node.children.back() : (const pcGrammarNode &) nullptr;
        auto type = getVarType(node.children.begin() + 1, node.children.end() - (initVal ? 2 : 0));
        if (current_function) {
            auto alloca_ = new Instruction::alloca_(type);
            if (auto msg = symbol_table.insert(identifier.raw, {alloca_, nullptr}, identifier)) {
                message_queue.push_back(*msg);
            }
            if (initVal == nullptr) return {nullptr, {alloca_}};
            auto [vec, list] = visitInitVal(*initVal);
            list.push_back(alloca_);
            list.splice(list.end(), storeInitValue(alloca_, type, vec.begin()));
            return {nullptr, list};
        } else {
            mir::Literal *literal;
            if (initVal) {
                auto [value, list] = visit<ConstInitVal>(*initVal);
                literal = dynamic_cast<mir::Literal *>(value);
                assert(literal && list.empty());
            } else {
                literal = nullptr;
            }
            auto variable = new mir::GlobalVar(type, std::string(identifier.raw), literal, false);
            manager.globalVars.push_back(variable);
            if (auto msg = symbol_table.insert(identifier.raw, {variable, nullptr}, identifier)) {
                message_queue.push_back(*msg);
            }
            return {};
        }
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<FuncDef>(const GrammarNode &node) {
        // funcType IDENFR LPARENT funcFParams? RPARENT block
        auto funcRetType = node.children.front()->children.front()->getToken().type == VOIDTK ?
                           mir::Type::getVoidType() : mir::Type::getI32Type();
        auto &identifier = node.children[1]->getToken();
        decltype(token_buffer)().swap(token_buffer);
        value_list params_list = node.children.size() == 6 ? visit(*node.children[3]).second : value_list{};
        std::vector<mir::pType> params;
        std::transform(params_list.begin(), params_list.end(), std::back_inserter(params),
                       [](value_type value) {
                           return value->getType();
                       });
        auto funcType = mir::FunctionType::getFunctionType(funcRetType, std::move(params));
        auto func = new mir::Function(funcType, std::string(identifier.raw));
        current_function = func;
        manager.functions.push_back(func);
        if (auto msg = symbol_table.insert(identifier.raw, {func, nullptr}, identifier)) {
            message_queue.push_back(*msg);
        }
        symbol_table.enter_cache_block();
        auto it1 = params_list.begin();
        auto it2 = token_buffer.begin();
        value_list init_list = {};
        for (; it1 != params_list.end(); ++it1, ++it2) {
            auto arg = func->addArgument((*it1)->getType());
            mir::Value *val = arg;
            if (arg->getType() == mir::Type::getI32Type()) {
                auto alloca_ = new Instruction::alloca_(mir::Type::getI32Type());
                auto store_ = new Instruction::store(arg, alloca_);
                init_list.push_back(alloca_);
                init_list.push_back(store_);
                val = alloca_;
            }
            if (auto msg = symbol_table.insert((*it1)->getName(), {val, nullptr}, **it2)) {
                message_queue.push_back(*msg);
            }
        }
        auto [_, list] = visit(*node.children.back());
        init_list.splice(init_list.end(), list);
        listToBB(init_list, node.children.back()->children.back()->getToken());
        return {};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<MainFuncDef>(const GrammarNode &node) {
        // INTTK MAINTK LPARENT RPARENT block
        auto func = new mir::Function(mir::FunctionType::getFunctionType(mir::Type::getI32Type(), {}), "main");
        current_function = func;
        manager.functions.push_back(func);
        auto [_, list] = visit(*node.children.back());
        listToBB(list, node.children.back()->children.back()->getToken());
        return {};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<FuncFParam>(const GrammarNode &node) {
        // bType IDENFR (LBRACK RBRACK (LBRACK constExp RBRACK)*)?
        auto &identifier = node.children[1]->getToken();
        auto type = getVarType(node.children.begin() + 2, node.children.end());
        auto virtual_value = new mir::Value(type);
        virtual_value->setName(std::string(identifier.raw));
        token_buffer.push_back(&identifier);
        return {nullptr, {virtual_value}};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<Block>(const GrammarNode &node) {
        // LBRACE blockItem* RBRACE
        symbol_table.enter_block();
        auto result = visitChildren(node);
        symbol_table.exit_block();
        return result;
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<IfStmt>(const GrammarNode &node) {
        // IFTK LPARENT cond RPARENT stmt (ELSETK stmt)?
        cond_stack.push({new mir::BasicBlock(), new mir::BasicBlock()});
        auto [cond_v, cond_l] = visit(*node.children[2]);
        auto [if_v, if_l] = visit(*node.children[4]);
        value_list list = {};
        list.splice(list.end(), cond_l);
        list.push_back(cond_stack.top().true_block);
        list.splice(list.end(), if_l);
        if (node.children.size() == 7) {
            auto [else_v, else_l] = visit(*node.children[6]);
            auto end_block = new mir::BasicBlock();
            list.push_back(new Instruction::br(end_block));
            list.push_back(cond_stack.top().false_block);
            list.splice(list.end(), else_l);
            list.push_back(end_block);
        } else {
            list.push_back(cond_stack.top().false_block);
        }
        cond_stack.pop();
        return {nullptr, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<WhileStmt>(const GrammarNode &node) {
        // WHILETK LPARENT cond RPARENT stmt
        auto &cond = node.children[2];
        auto &stmt = node.children[4];
        // (br) -> (true) -> stmt -> (continue) -> cond -> (break/false)
        loop_stack.push({new mir::BasicBlock(), new mir::BasicBlock()});
        cond_stack.push({new mir::BasicBlock(), loop_stack.top().break_block});
        value_list list = {new Instruction::br(loop_stack.top().continue_block)};
        list.push_back(cond_stack.top().true_block);
        auto [stmt_v, stmt_l] = visit(*stmt);
        list.splice(list.end(), stmt_l);
        list.push_back(loop_stack.top().continue_block);
        auto [cond_v, cond_l] = visit(*cond);
        list.splice(list.end(), cond_l);
        list.push_back(cond_stack.top().false_block);
        cond_stack.pop();
        loop_stack.pop();
        return {nullptr, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ForLoopStmt>(const GrammarNode &node) {
        // FORTK LPARENT forStmt? SEMICN cond? SEMICN forStmt? RPARENT stmt
        auto is_semicn = [](const pcGrammarNode &ptr) {
            return ptr->type == Terminal && ptr->getToken().type == SEMICN;
        };
        auto semicn1 = std::find_if(node.children.begin(), node.children.end(), is_semicn);
        auto semicn2 = std::find_if(semicn1 + 1, node.children.end(), is_semicn);
        auto forStmt1_it = std::find_if(node.children.begin(), semicn1, &is_specific_type<ForStmt>);
        auto cond_it = std::find_if(semicn1 + 1, semicn2, &is_specific_type<Cond>);
        auto forStmt2_it = std::find_if(semicn2 + 1, node.children.end(), &is_specific_type<ForStmt>);
        auto &forStmt1 = forStmt1_it == semicn1 ? (const pcGrammarNode &) nullptr : *forStmt1_it;
        auto &cond = cond_it == semicn2 ? (const pcGrammarNode &) nullptr : *cond_it;
        auto &forStmt2 = forStmt2_it == node.children.end() ? (const pcGrammarNode &) nullptr : *forStmt2_it;
        auto &stmt = node.children.back();
        // forStmt1 -> (br) -> (true) -> stmt -> (continue) -> forStmt2 -> (cond) -> cond -> (break/false)
        auto cond_block = new mir::BasicBlock();
        loop_stack.push({new mir::BasicBlock(), new mir::BasicBlock()});
        cond_stack.push({new mir::BasicBlock(), loop_stack.top().break_block});
        value_list list = {};
        if (forStmt1) {
            auto [v, l] = visit(*forStmt1);
            list.splice(list.end(), l);
        }
        list.push_back(new Instruction::br(cond_block));
        list.push_back(cond_stack.top().true_block);
        auto [_v, _l] = visit(*stmt);
        list.splice(list.end(), _l);
        list.push_back(loop_stack.top().continue_block);
        if (forStmt2) {
            auto [v, l] = visit(*forStmt2);
            list.splice(list.end(), l);
        }
        list.push_back(cond_block);
        if (cond) {
            auto [v, l] = visit(*cond);
            list.splice(list.end(), l);
        } else {
            auto br = new Instruction::br(cond_stack.top().true_block);
            list.push_back(br);
        }
        list.push_back(cond_stack.top().false_block);
        cond_stack.pop();
        loop_stack.pop();
        return {nullptr, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<AssignStmt>(const GrammarNode &node) {
        // lVal ASSIGN exp SEMICN
        auto [variable, list] = visit(*node.children[0]);
        auto [value, l] = visit(*node.children[2]);
        list.splice(list.end(), l);
        if (variable->isConst()) {
            auto &ident = node.children[0]->children[0]->getToken();
            message_queue.push_back(message{
                    message::ERROR, 'h', ident.line, ident.column,
                    "cannot assign to const variable"
            });
            return {nullptr, list};
        }
        auto store = new Instruction::store(value, variable);
        list.push_back(store);
        return {store, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<BreakStmt>(const GrammarNode &node) {
        // BREAKTK SEMICN
        auto &breaktk = node.children[0]->getToken();
        if (loop_stack.empty()) {
            message_queue.push_back(message{
                    message::ERROR, 'm', breaktk.line, breaktk.column,
                    "break statement not within a loop"
            });
            return {nullptr, {}};
        }
        return {nullptr, {new Instruction::br(loop_stack.top().break_block)}};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ContinueStmt>(const GrammarNode &node) {
        // CONTINUETK SEMICN
        auto &continuetk = node.children[0]->getToken();
        if (loop_stack.empty()) {
            message_queue.push_back(message{
                    message::ERROR, 'm', continuetk.line, continuetk.column,
                    "continue statement not within a loop"
            });
            return {nullptr, {}};
        }
        return {nullptr, {new Instruction::br(loop_stack.top().continue_block)}};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ReturnStmt>(const GrammarNode &node) {
        // RETURNTK exp? SEMICN
        auto &returntk = node.children[0]->getToken();
        if (current_function->retType == mir::Type::getVoidType() && node.children.size() == 3) {
            message_queue.push_back(message{
                    message::ERROR, 'f', returntk.line, returntk.column,
                    "void function should not return a value"
            });
            return {nullptr, {}};
        } else if (current_function->retType != mir::Type::getVoidType() && node.children.size() == 2) {
            message_queue.push_back(message{
                    message::ERROR, 'f', returntk.line, returntk.column,
                    "Non-void function should return a value"
            });
            return {nullptr, {}};
        }
        if (node.children.size() == 2) return {nullptr, {new Instruction::ret()}};
        auto [value, list] = visit(*node.children[1]);
        list.push_back(new Instruction::ret(value));
        return {nullptr, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<GetintStmt>(const GrammarNode &node) {
        // lVal ASSIGN GETINTTK LPARENT RPARENT SEMICN
        auto [variable, list] = visit(*node.children[0]);
        auto call = new Instruction::call(mir::Function::getint, {});
        list.push_back(call);
        if (variable->isConst()) {
            auto &ident = node.children[0]->children[0]->getToken();
            message_queue.push_back(message{
                    message::ERROR, 'h', ident.line, ident.column,
                    "cannot assign to const variable"
            });
            return {nullptr, list};
        }
        auto store = new Instruction::store(call, variable);
        list.push_back(store);
        return {store, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<PrintfStmt>(const GrammarNode &node) {
        // PRINTFTK LPARENT STRCON (COMMA exp)* RPARENT SEMICN
        auto &strcon = node.children[2]->getToken();
        if (auto msg = check_valid(strcon)) {
            message_queue.push_back(*msg);
            return {};
        }
        auto count_params = std::count_if(node.children.begin(), node.children.end(), &is_specific_type<Exp>);
        auto count_format = std::count(strcon.raw.begin(), strcon.raw.end(), '%');
        if (count_format != count_params) {
            using namespace std::string_literals;
            std::string msg =
                    "too "s + (count_params < count_format ? "few" : "many") +
                    " arguments for format " + std::string(strcon.raw) +
                    ", expected " + std::to_string(count_format) +
                    " but " + std::to_string(count_params) + " given";
            auto &printftk = node.children[0]->getToken();
            message_queue.push_back(message{message::ERROR, 'l', printftk.line, printftk.column, msg});
            return {};
        }
        auto [exps, list] = visitExps(node.children.begin() + 3, node.children.end() - 2);
        auto exps_it = exps.begin();
        auto str = std::string(strcon.raw.substr(1, strcon.raw.size() - 2));
        size_t pos;
        while ((pos = str.find("\\n")) != std::string::npos) {
            str.replace(pos, 2, "\n");
        }
        auto sv = std::string_view(str);
        while (!sv.empty()) {
            pos = sv.find("%d");
            if (pos != 0) {
                auto s = std::string(sv.substr(0, pos));
                if (s.size() == 1) {
                    auto literal = manager.getIntegerLiteral(s[0]);
                    list.push_back(new Instruction::call(mir::Function::putch, {literal}));
                } else {
                    auto var = manager.getStringLiteral(s);
                    list.push_back(new Instruction::call(mir::Function::putstr, {var}));
                }
            }
            if (pos == std::string_view::npos) break;
            list.push_back(new Instruction::call(mir::Function::putint, {*exps_it++}));
            sv = sv.substr(pos + 2);
        }
        return {nullptr, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ForStmt>(const GrammarNode &node) {
        return visit<AssignStmt>(node);
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<LVal>(const frontend::grammar::GrammarNode &node) {
        // IDENFR (LBRACK exp RBRACK)*
        auto &identifier = node.children[0]->getToken();
        auto [variable, literal] = symbol_table.lookup(identifier.raw);
        if (variable == nullptr) {
            message_queue.push_back(message{
                    message::ERROR, 'c', identifier.line, identifier.column,
                    "undefined symbol '" + std::string(identifier.raw) + "'"
            });
            return {undefined, {}};
        }
        auto [indices, l] = visitExps(node.children.begin() + 1, node.children.end(), {zero_value});
        if (in_const_expr) {
            if (node.children.size() > 1) {
                assert(l.empty());
                for (int i = 1; i < indices.size(); i++) {
                    auto lit = dynamic_cast<mir::IntegerLiteral *>(indices[i]);
                    auto arr = dynamic_cast<mir::ArrayLiteral *>(literal);
                    assert(lit && literal);
                    literal = arr->values[lit->value];
                }
            }
            return {literal, {}};
        } else {
            value_list list = {};
            if (node.children.size() > 1) {
                list.splice(list.end(), l);
                bool isConst = variable->isConst();
                auto ty = variable->getType();
                for (int i = 1; i < indices.size(); i++) ty = ty->getBase();
                variable = new Instruction::getelementptr(ty, variable, indices);
                variable->setConst(isConst);
                list.push_back(variable);
            }
            return {variable, list};
        }
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<PrimaryExp>(const frontend::grammar::GrammarNode &node) {
        // LPARENT exp RPARENT | lVal | number
        if (in_const_expr) return visitChildren(node);
        auto [value, list] = visitChildren(node);
        if (node.children[0]->type == LVal && value->getType() == mir::Type::getI32Type()) {
            value = new Instruction::load(mir::Type::getI32Type(), value);
            list.push_back(value);
        }
        return {value, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<Number>(const frontend::grammar::GrammarNode &node) {
        // INTCON
        auto &token = node.children[0]->getToken();
        auto value = std::stoi(token.raw.data());
        auto literal = manager.getIntegerLiteral(value);
        return {literal, {}};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<UnaryExp>(const frontend::grammar::GrammarNode &node) {
        // primaryExp | IDENFR LPARENT funcRParams? RPARENT | unaryOp unaryExp
        if (node.children[0]->type == PrimaryExp) return visit(*node.children[0]);
        if (node.children[0]->type == Terminal) {
            auto &identifier = node.children[0]->getToken();
            auto variable = symbol_table.lookup(identifier.raw).first;
            if (variable == nullptr) {
                message_queue.push_back(message{
                        message::ERROR, 'c', identifier.line, identifier.column,
                        "undefined symbol '" + std::string(identifier.raw) + "'"
                });
                return {zero_value, {}};
            }
            auto function = dynamic_cast<mir::Function *>(variable);
            if (function == nullptr) {
                message_queue.push_back(message{
                        message::ERROR, 'c', identifier.line, identifier.column,
                        "symbol '" + std::string(identifier.raw) + "' is not a function"
                });
                return {zero_value, {}};
            }
            value_list list = {};
            if (node.children.size() == 3) {
                auto call = new Instruction::call(function, {});
                list.push_back(call);
                return {call, list};
            }
            // visit funcRParams
            auto [value, l] = visitExps(node.children[2]->children.begin(), node.children[2]->children.end());
            if (function->getType()->getFunctionParamCount() != value.size()) {
                message_queue.push_back(message{
                        message::ERROR, 'd', identifier.line, identifier.column,
                        std::string("too ") +
                        (function->getType()->getFunctionParamCount() < value.size() ? "many" : "few") +
                        " arguments for function '" + std::string(identifier.raw) + "'"
                });
            } else {
                for (int i = 0; i < value.size(); i++) {
                    if (!value[i]->getType()->convertableTo(function->getType()->getFunctionParam(i))) {
                        message_queue.push_back(message{
                                message::ERROR, 'e', identifier.line, identifier.column,
                                std::string("incompatible type for the ") + std::to_string(i + 1) +
                                "th argument of function '" + std::string(identifier.raw) + "'"
                        });
                    }
                }
            }
            list.splice(list.end(), l);
            auto call = new Instruction::call(function, value);
            list.push_back(call);
            return {call, list};
        }
        auto [value, list] = visit(*node.children[1]);
        auto &unary_op = node.children[0]->children[0]->getToken();
        if (in_const_expr) {
            auto literal = dynamic_cast<mir::IntegerLiteral *>(value);
            assert(literal && list.empty());
            if (unary_op.type == MINU) {
                literal = manager.getIntegerLiteral(-literal->value);
            } else {
                assert(unary_op.type == PLUS);
            }
            return {literal, {}};
        }
        switch (unary_op.type) {
        case PLUS:
            break;
        case MINU:
            value = new Instruction::sub(zero_value, value);
            list.push_back(value);
            break;
        case NOT:
            value = new Instruction::icmp(Instruction::icmp::EQ, value, zero_value);
            list.push_back(value);
            value = new Instruction::zext(mir::Type::getI32Type(), value);
            list.push_back(value);
            break;
        default:
            assert(false);
        }
        return {value, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<MulExp>(const frontend::grammar::GrammarNode &node) {
        // unaryExp ((MULT | DIV | MOD) unaryExp)*
        return visitBinaryExp(node);
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<AddExp>(const frontend::grammar::GrammarNode &node) {
        // mulExp ((PLUS | MINU) mulExp)*
        return visitBinaryExp(node);
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<RelExp>(const frontend::grammar::GrammarNode &node) {
        // addExp ((LSS | LEQ | GRE | GEQ) addExp)*
        return visitBinaryExp(node);
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<EqExp>(const frontend::grammar::GrammarNode &node) {
        // relExp ((EQL | NEQ) relExp)*
        return visitBinaryExp(node);
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<LAndExp>(const frontend::grammar::GrammarNode &node) {
        // eqExp (AND eqExp)*
        auto [value, list] = visit(*node.children[0]);
        value = truncToI1(value, list);
        for (auto it = node.children.begin() + 1; it != node.children.end(); ++it) {
            if ((*it)->type == Terminal) continue;
            auto bb = new mir::BasicBlock();
            list.push_back(new Instruction::br(value, bb, cond_stack.top().false_block));
            list.push_back(bb);
            auto [v, l] = visit(**it);
            value = truncToI1(v, l);
            list.splice(list.end(), l);
        }
        list.push_back(new Instruction::br(value, cond_stack.top().true_block, cond_stack.top().false_block));
        return {value, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<LOrExp>(const frontend::grammar::GrammarNode &node) {
        // lAndExp (OR lAndExp)*
        auto count = std::count_if(node.children.begin(), node.children.end(), &is_specific_type<LAndExp>);
        value_type value = nullptr;
        value_list list = {};
        for (int i = 0; i < count; i++) {
            auto &lAndExp = *node.children[i * 2];
            mir::BasicBlock *false_block = i + 1 == count ? cond_stack.top().false_block : new mir::BasicBlock();
            cond_stack.push({cond_stack.top().true_block, false_block});
            auto [v, l] = visit(lAndExp);
            value = truncToI1(v, l);
            list.splice(list.end(), l);
            if (i + 1 != count) list.push_back(false_block);
            cond_stack.pop();
        }
        return {value, list};
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ConstExp>(const frontend::grammar::GrammarNode &node) {
        // addExp
        in_const_expr = true;
        auto [value, list] = visit(*node.children[0]);
        in_const_expr = false;
        return {value, list};
    }
}

// visitor: default methods
namespace frontend::visitor {
    SysYVisitor::return_type SysYVisitor::visit(const GrammarNode &node) {
        using namespace grammar_type;
        static decltype(&SysYVisitor::visit<CompUnit>) methods[] =
                {&SysYVisitor::visit<CompUnit>, &SysYVisitor::visit<Decl>, &SysYVisitor::visit<ConstDecl>,
                 &SysYVisitor::visit<BType>, &SysYVisitor::visit<ConstDef>, &SysYVisitor::visit<ConstInitVal>,
                 &SysYVisitor::visit<VarDecl>, &SysYVisitor::visit<VarDef>, &SysYVisitor::visit<InitVal>,
                 &SysYVisitor::visit<FuncDef>, &SysYVisitor::visit<MainFuncDef>, &SysYVisitor::visit<FuncType>,
                 &SysYVisitor::visit<FuncFParams>, &SysYVisitor::visit<FuncFParam>, &SysYVisitor::visit<Block>,
                 &SysYVisitor::visit<BlockItem>, &SysYVisitor::visit<Stmt>, &SysYVisitor::visit<AssignStmt>,
                 &SysYVisitor::visit<ExpStmt>, &SysYVisitor::visit<BlockStmt>, &SysYVisitor::visit<IfStmt>,
                 &SysYVisitor::visit<WhileStmt>, &SysYVisitor::visit<ForLoopStmt>, &SysYVisitor::visit<BreakStmt>,
                 &SysYVisitor::visit<ContinueStmt>, &SysYVisitor::visit<ReturnStmt>, &SysYVisitor::visit<GetintStmt>,
                 &SysYVisitor::visit<PrintfStmt>, &SysYVisitor::visit<ForStmt>, &SysYVisitor::visit<Exp>,
                 &SysYVisitor::visit<Cond>, &SysYVisitor::visit<LVal>, &SysYVisitor::visit<PrimaryExp>,
                 &SysYVisitor::visit<Number>, &SysYVisitor::visit<UnaryExp>, &SysYVisitor::visit<UnaryOp>,
                 &SysYVisitor::visit<FuncRParams>, &SysYVisitor::visit<MulExp>, &SysYVisitor::visit<AddExp>,
                 &SysYVisitor::visit<RelExp>, &SysYVisitor::visit<EqExp>, &SysYVisitor::visit<LAndExp>,
                 &SysYVisitor::visit<LOrExp>, &SysYVisitor::visit<ConstExp>, &SysYVisitor::visit<Terminal>};
        return (this->*methods[node.type])(node);
    }

    SysYVisitor::return_type SysYVisitor::visitChildren(const GrammarNode &node) {
        value_type value{};
        value_list list{};
        for (const auto &ptr: node.children) {
            auto [r1, r2] = visit(*ptr);
            value = r1 ? r1 : value;
            list.splice(list.end(), r2);
        }
        return {value, list};
    }

    // default: do nothing
    template<grammar_type_t type>
    SysYVisitor::return_type SysYVisitor::visit(const GrammarNode &node) {
        return visitChildren(node);
    }
}
