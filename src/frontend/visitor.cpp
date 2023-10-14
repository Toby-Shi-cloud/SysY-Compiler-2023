//
// Created by toby on 2023/9/22.
//

#include <iostream>
#include "visitor.h"
#include "../mir/instruction.h"

namespace frontend::visitor {
    std::optional<message> SymbolTable::insert(std::string_view name, store_type_t value, const lexer::Token &token) {
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

    inline std::optional<message> check_valid(const lexer::Token &str_token) {
        assert(str_token.type == lexer::token_type::STRCON);
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
                } else if (ch != 32 && ch != 33 && ch < 40 && ch > 126) {
                    return message{message::ERROR, 'a', line, column, "invalid character '"s + ch + "'"};
                }
            }
        }
        if (backslash)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2, "invalid escape sequence '\\'"};
        if (percent)
            return message{message::ERROR, 'a', str_token.line, str_token.column + str_token.raw.size() - 2, "invalid format specifier '%'"};
        return std::nullopt;
    }
}

// visitor: helper methods
namespace frontend::visitor {
    using namespace grammar_type;
    using namespace lexer::token_type;
    using lexer::token_type_t;
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
                auto literal = dynamic_cast<mir::Literal *>(value);
                assert(literal && list.empty());
                result = mir::ArrayType::getArrayType(literal->getInt(), result);
            }
        }

        return result;
    }

    SysYVisitor::return_type SysYVisitor::visitBinaryExp(const GrammarNode &node) {
        using literal_operator = decltype(&mir::literal_operators::operator+);
        using normal_operator = decltype(&get_binary_instruction<Instruction::add>);
        using namespace mir::literal_operators;
        using icmp = mir::Instruction::icmp;
        static std::unordered_map<token_type_t, std::pair<literal_operator, normal_operator>> call_table = {
                {PLUS, {&operator+, &get_binary_instruction<Instruction::add>}},
                {MINU, {&operator-, &get_binary_instruction<Instruction::sub>}},
                {MULT, {&operator*, &get_binary_instruction<Instruction::mul>}},
                {DIV, {&operator/, &get_binary_instruction<Instruction::sdiv>}},
                {MOD, {&operator%, &get_binary_instruction<Instruction::srem>}},
                {LSS, {nullptr, &get_icmp_instruction<icmp::SLT>}},
                {LEQ, {nullptr, &get_icmp_instruction<icmp::SLE>}},
                {GRE, {nullptr, &get_icmp_instruction<icmp::SGT>}},
                {GEQ, {nullptr, &get_icmp_instruction<icmp::SGE>}},
                {EQL, {nullptr, &get_icmp_instruction<icmp::EQ>}},
                {NEQ, {nullptr, &get_icmp_instruction<icmp::NE>}},
        };

        auto [ret_val, ret_list] = visit(*node.children[0]);
        for (auto it = node.children.begin() + 1; it != node.children.end(); it += 2) {
            auto &token = (*it)->getToken();
            auto [value, list] = visit(*it[1]);
            ret_list.merge(list);

            auto ret_literal = dynamic_cast<mir::Literal *>(ret_val);
            auto literal = dynamic_cast<mir::Literal *>(value);

            auto [f, g] = call_table[token.type];

            if (f && ret_literal && literal && ret_list.empty()) {
                ret_literal = new mir::Literal(f(*ret_literal, *literal));
                manager.literalPool.insert(ret_literal);
            }
            else {
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
                           list.merge(l);
                           return value;
                       });
        indices.erase(std::remove_if(indices.begin(), indices.end(), [](value_type ptr) { return ptr == nullptr; }),
                      indices.end());
        return {indices, list};
    }
}

// visitor: specific methods
namespace frontend::visitor {
    //TODO: IfStmt, ForLoopStmt, LAndExp, LOrExp, ConstDef, VarDef, FuncDef

    template<>
    SysYVisitor::return_type SysYVisitor::visit<ConstDef>(const GrammarNode &node) {
        // IDENFR (LBRACK constExp RBRACK)* ASSIGN constInitVal
        auto &identifier = node.children[0]->getToken();
        auto type = getVarType(node.children.begin() + 1, node.children.end() - 2);
        if (current_function) {
            return generateLocalVar(type, identifier, node.children.back(), true);
        } else {
            auto [value, list] = visit(*node.children.back());
            auto literal = dynamic_cast<mir::Literal *>(value);
            assert(literal && list.empty());
            auto variable = new mir::GlobalVar(type, std::string(identifier.raw), literal, true);
            manager.globalVars.push_back(variable);
            if (auto msg = symbol_table.insert(identifier.raw, {variable, literal}, identifier)) {
                message_queue.push_back(*msg);
            }
            return {};
        }
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
    SysYVisitor::return_type SysYVisitor::visit<AssignStmt>(const GrammarNode &node) {
        // lVal ASSIGN exp SEMICN
        auto [variable, list] = visit(*node.children[0]);
        auto [value, l] = visit(*node.children[2]);
        list.merge(l);
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
                auto literal = new mir::Literal(mir::make_literal(s));
                manager.literalPool.insert(literal);
                auto var = new mir::GlobalVar(mir::Type::getStringType((int)s.length() + 1), literal, true);
                manager.globalVars.push_back(var);
                list.push_back(new Instruction::call(mir::Function::putstr, {var}));
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
            return {zero_value, {}};
        }
        auto [indices, l] = visitExps(node.children.begin() + 1, node.children.end(), {zero_value});
        if (in_const_expr) {
            auto result = *literal;
            if (node.children.size() > 1) {
                assert(l.empty());
                for (int i = 1; i < indices.size(); i++) {
                    auto lit = dynamic_cast<mir::Literal *>(indices[i]);
                    assert(lit);
                    result = result.getArray()[lit->getInt()];
                }
                literal = new mir::Literal(result);
                manager.literalPool.insert(literal);
            }
            return {literal, {}};
        } else {
            value_list list = {};
            if (node.children.size() > 1) {
                list.merge(l);
                bool isConst = variable->isConst();
                auto ty = variable->getType();
                for (int i = 1; i < indices.size(); i++) ty = ty->getBase();
                variable = new Instruction::getelementptr(ty, variable, indices);
                variable->setConst(isConst);
            }
            return {variable, list};
        }
    }

    template<>
    SysYVisitor::return_type SysYVisitor::visit<PrimaryExp>(const frontend::grammar::GrammarNode &node) {
        // LPARENT exp RPARENT | lVal | number
        if (in_const_expr) return visitChildren(node);
        auto [value, list] = visitChildren(node);
        if (node.children[0]->type == LVal) {
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
        auto literal = new mir::Literal(mir::make_literal(value));
        manager.literalPool.insert(literal);
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
            list.merge(l);
            auto call = new Instruction::call(function, value);
            list.push_back(call);
            return {call, list};
        }
        auto [value, list] = visit(*node.children[1]);
        auto &unary_op = node.children[0]->children[0]->getToken();
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

    SysYVisitor::return_type SysYVisitor::visitChildren(const GrammarNode &node) {
        value_type value{};
        value_list list{};
        for (const auto &ptr: node.children) {
            auto [r1, r2] = visit(*ptr);
            value = r1 ? r1 : value;
            list.merge(r2);
        }
        return {value, list};
    }

    // default: do nothing
    template<grammar_type_t type>
    SysYVisitor::return_type SysYVisitor::visit(const GrammarNode &node) {
        return visitChildren(node);
    }
}
