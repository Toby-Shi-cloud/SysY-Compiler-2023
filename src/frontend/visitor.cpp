//
// Created by toby on 2023/9/22.
//

#include "frontend/visitor.h"
#include <iostream>

// Symbol Table
namespace frontend::visitor {
std::optional<message> SymbolTable::insert(std::string_view name, const store_type_t &value,
                                           const Token &token) {
    if (stack.back().count(name))
        return message{message::ERROR, 'b', token.line, token.column,
                       "redefinition of '" + std::string(name) + "'"};
    stack.back()[name] = value;
    return std::nullopt;
}

SymbolTable::store_type_t SymbolTable::lookup(std::string_view name) {
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (it->count(name)) return it->at(name);
    }
    return {};
}
}  // namespace frontend::visitor

// visitor: helper functions
namespace frontend::visitor {
template <grammar_type_t type>
bool is_specific_type(const pcGrammarNode &node) {
    return node->type == type;
}

template <typename T>
mir::Instruction *constructor_wrapper(mir::Value *lhs, mir::Value *rhs) {
    return new T{lhs, rhs};
}

template <mir::Instruction::icmp::Cond cond>
mir::Instruction *constructor_wrapper(mir::Value *lhs, mir::Value *rhs) {
    return new mir::Instruction::icmp{cond, lhs, rhs};
}

template <mir::Instruction::fcmp::Cond cond>
mir::Instruction *constructor_wrapper(mir::Value *lhs, mir::Value *rhs) {
    return new mir::Instruction::fcmp{cond, lhs, rhs};
}

mir::Value *convert_to(mir::Value *value, mir::pType ty, bool sgn,
                       /*out*/ std::list<mir::Value *> &list) {
    if (value->type == ty) return value;
    while (auto arr = dynamic_cast<mir::ArrayValue *>(value)) value = arr->values[0];
    if (value->type == ty) return value;
    if (!value->type->isNumberTy()) return value;
    if (value->type == mir::Type::getI1Type()) sgn = false;  // bool is unsigned
    if (auto lit = dynamic_cast<mir::Literal *>(value)) {
        if (ty->isIntegerTy()) {
            return mir::getIntegerLiteral(int(lit->getValue()));
        } else {
            return mir::getFloatLiteral(float(lit->getValue()));
        }
    }
    mir::Value *ret;
    if (value->type->isIntegerTy()) {
        if (ty->isIntegerTy()) {
            assert(value->type->getIntegerBits() != ty->getIntegerBits());
            if (value->type->getIntegerBits() > ty->getIntegerBits()) {
                ret = new mir::Instruction::trunc(ty, value);
            } else {
                ret = sgn ? (mir::Instruction *)new mir::Instruction::sext(ty, value)
                          : (mir::Instruction *)new mir::Instruction::zext(ty, value);
            }
        } else {
            ret = sgn ? (mir::Instruction *)new mir::Instruction::sitofp(ty, value)
                      : (mir::Instruction *)new mir::Instruction::uitofp(ty, value);
        }
    } else {
        if (ty->isIntegerTy()) {
            ret = sgn ? (mir::Instruction *)new mir::Instruction::fptosi(ty, value)
                      : (mir::Instruction *)new mir::Instruction::fptoui(ty, value);
        } else {
            __builtin_unreachable();
        }
    }
    list.push_back(ret);
    return ret;
}

mir::Value *convert_to_bool(mir::Value *value, bool neg, /*out*/ std::list<mir::Value *> &list) {
    if (value->type == mir::Type::getI1Type()) {
        if (!neg) return value;
        auto inst = new mir::Instruction::xor_(value, mir::getBooleanLiteral(true));
        list.push_back(inst);
        return inst;
    }
    mir::Value *cmp;
    if (value->type->isIntegerTy()) {
        using icmp = mir::Instruction::icmp;
        cmp = new icmp(neg ? icmp::EQ : icmp::NE, value, mir::getLiteral(0));
    } else {
        assert(value->type->isFloatTy());
        using fcmp = mir::Instruction::fcmp;
        cmp = new fcmp(neg ? fcmp::OEQ : fcmp::ONE, value, mir::getLiteral(.0f));
    }
    list.push_back(cmp);
    return cmp;
}
}  // namespace frontend::visitor

// visitor: helper methods
namespace frontend::visitor {
using namespace grammar_type;
using namespace token_type;
using mir::Instruction;

static const pcGrammarNode nullptr_node{nullptr};

mir::pType SysYVisitor::getVarType(GrammarIterator begin, GrammarIterator end) {
    mir::pType result = current_btype;

    for (auto it = std::reverse_iterator(end); it != std::reverse_iterator(begin); ++it) {
        auto &node = *it;
        if (node->type != Terminal) continue;
        auto token_type = node->getToken().type;
        if (token_type == RBRACK) continue;
        assert(token_type == LBRACK);
        if (auto &exp = *(it - 1); exp->type == Terminal) {
            result = mir::PointerType::getPointerType(result);
        } else {
            assert(exp->type == ConstExp);
            auto [value, list] = visit(*exp);
            auto literal = dynamic_cast<mir::IntegerLiteral *>(value);
            assert(literal && list.empty());
            result = mir::ArrayType::getArrayType(literal->value, result);
        }
    }

    return result;
}

SysYVisitor::return_type SysYVisitor::visitBinaryExp(const GrammarNode &node) {
    using lit_op = mir::Literal *(*)(const mir::Literal &, const mir::Literal &);
    using constructor = mir::Instruction *(*)(mir::Value *, mir::Value *);
    static std::unordered_map<token_type_t, std::tuple<lit_op, constructor, constructor>>
        call_table = {
#define WRAP(op) (&constructor_wrapper<mir::Instruction::op>)
            {PLUS, {&mir::operator+, WRAP(add), WRAP(fadd)}},
            {MINU, {&mir::operator-, WRAP(sub), WRAP(fsub)}},
            {MULT, {&mir::operator*, WRAP(mul), WRAP(fmul)}},
            {DIV, {&mir::operator/, WRAP(sdiv), WRAP(fdiv)}},
            {MOD, {&mir::operator%, WRAP(srem), WRAP(frem)}},
            {LSS, {&mir::operator<, WRAP(icmp::SLT), WRAP(fcmp::OLT)}},
            {LEQ, {&mir::operator<=, WRAP(icmp::SLE), WRAP(fcmp::OLE)}},
            {GRE, {&mir::operator>, WRAP(icmp::SGT), WRAP(fcmp::OGT)}},
            {GEQ, {&mir::operator>=, WRAP(icmp::SGE), WRAP(fcmp::OGE)}},
            {EQL, {&mir::operator==, WRAP(icmp::EQ), WRAP(fcmp::OEQ)}},
            {NEQ, {&mir::operator!=, WRAP(icmp::NE), WRAP(fcmp::ONE)}},
#undef WRAP
        };

    auto [ret_val, ret_list] = visit(*node.children[0]);
    for (auto it = node.children.begin() + 1; it != node.children.end(); it += 2) {
        auto token_type = (*it)->getToken().type;
        auto [value, list] = visit(*it[1]);
        ret_list.splice(ret_list.end(), list);

        auto ret_literal = dynamic_cast<mir::Literal *>(ret_val);
        auto literal = dynamic_cast<mir::Literal *>(value);

        auto [lit, funci, funcf] = call_table[token_type];
        if (ret_literal && literal && ret_list.empty()) {
            ret_literal = lit(*ret_literal, *literal);
            ret_val = ret_literal;
        } else if (ret_val->type->isFloatTy() || value->type->isFloatTy()) {
            ret_val = convert_to(ret_val, mir::Type::getFloatType(), true, ret_list);
            value = convert_to(value, mir::Type::getFloatType(), true, ret_list);
            ret_val = funcf(ret_val, value);
            ret_list.push_back(ret_val);
        } else {
            ret_val = convert_to(ret_val, mir::Type::getI32Type(), true, ret_list);
            value = convert_to(value, mir::Type::getI32Type(), true, ret_list);
            ret_val = funci(ret_val, value);
            ret_list.push_back(ret_val);
        }
    }
    return {ret_val, ret_list};
}

std::pair<SysYVisitor::value_vector, SysYVisitor::value_list> SysYVisitor::visitExps(
    GrammarIterator begin, GrammarIterator end, value_vector init_value) {
    value_vector &indices = init_value;
    value_list list = {};
    std::transform(begin, end, std::back_inserter(indices),
                   [this, &list](const pcGrammarNode &ptr) -> value_type {
                       if (ptr->type == Terminal) return nullptr;
                       auto [value, l] = visit(*ptr);
                       list.splice(list.end(), l);
                       return value;
                   });
    indices.erase(std::remove_if(indices.begin(), indices.end(),
                                 [](value_type ptr) { return ptr == nullptr; }),
                  indices.end());
    return {indices, list};
}

SysYVisitor::value_list SysYVisitor::storeInitValue(value_type var, mir::pType type,
                                                    value_type initVal, value_vector *indices) {
    static value_vector _idx = std::vector<value_type>{zero_value};
    if (auto zero = dynamic_cast<mir::ZeroInitializer *>(initVal)) {
        auto ptr = new Instruction::getelementptr(type, var,
                                                  indices ? *indices : value_vector{zero_value});
        auto init = new Instruction::memset(ptr, 0, (int)zero->type->ssize());
        return {ptr, init};
    }
    if (type->isNumberTy()) {
        value_list list{};
        initVal = convert_to(initVal, type, true, list);
        if (indices) {
            var = new Instruction::getelementptr(type, var, *indices);
            list.push_back(var);
        }
        auto store = new Instruction::store(initVal, var);
        list.push_back(store);
        return list;
    }
    indices = indices ? indices : &_idx;
    auto [v, list] = array_cast(dynamic_cast<mir::ArrayValue *>(initVal), type);
    if (auto zero = dynamic_cast<mir::ZeroInitializer *>(v)) {
        auto ptr = new Instruction::getelementptr(type, var, *indices);
        auto init = new Instruction::memset(ptr, 0, (int)zero->type->ssize());
        list.push_back(ptr);
        list.push_back(init);
        return list;
    }
    auto arr = dynamic_cast<mir::ArrayValue *>(v);
    assert(arr);
    auto vec = arr->values;
    for (int i = 0; i < type->getArraySize(); i++) {
        if (mir::isZero(vec[i])) {
            int j = i + 1;
            for (; j < type->getArraySize(); j++)
                if (mir::isZero(vec[j]))
                    vec[j] = nullptr;
                else
                    break;
            vec[i] = mir::getZero(mir::ArrayType::getArrayType(j - i, type->getArrayBase()));
            i = j;
        }
    }
    for (int i = 0; i < type->getArraySize(); i++) {
        if (vec[i] == nullptr) continue;
        auto lit = mir::getIntegerLiteral(i);
        indices->push_back(lit);
        auto _l = storeInitValue(var, type->getArrayBase(), vec[i], indices);
        list.splice(list.end(), _l);
        indices->pop_back();
    }
    return list;
}

template <typename ArrTy, typename ValTy>
SysYVisitor::return_type SysYVisitor::array_cast(ArrTy *arr, mir::pType ty) {
    using vec_t = std::vector<ValTy>;
    if (arr == nullptr || arr->values.empty()) {
        return {mir::getZero(ty), {}};
    }
    value_list list{};
    if (!ty->isArrayTy()) {
        auto val = convert_to(arr, ty, true, list);
        return {val, list};
    }

    std::vector<mir::pType> tys{ty};
    std::vector<int> sizes{ty->getArraySize()};
    std::vector<std::unique_ptr<vec_t>> stack{};
    stack.emplace_back(new vec_t{});
    for (auto val : arr->values) {
        if (val->type->isArrayTy()) {
            auto [v, l] = array_cast(dynamic_cast<ArrTy *>(val), tys.back()->getArrayBase());
            list.splice(list.end(), l);
            stack.back()->push_back((ValTy)v);
        } else {
            while (tys.back()->getArrayBase()->isArrayTy()) {
                tys.push_back(tys.back()->getArrayBase());
                sizes.push_back(tys.back()->getArraySize());
                stack.emplace_back(new vec_t{});
            }
            stack.back()->push_back(val);
        }
        while (stack.back()->size() == sizes.back()) {
            auto v = new ArrTy(*stack.back().release());
            stack.pop_back(), sizes.pop_back(), tys.pop_back();
            if (stack.empty()) return {v, list};
            stack.back()->push_back(v);
        }
    }
    while (true) {
        auto ptr = stack.back().release();
        ptr->resize(sizes.back(), mir::getZero(tys.back()->getArrayBase()));
        auto v = new ArrTy(std::move(*ptr));
        stack.pop_back(), sizes.pop_back(), tys.pop_back();
        if (stack.empty()) return {v, list};
        stack.back()->push_back(v);
    }
}

void SysYVisitor::listToBB(value_list &list, const Token &end_token) const {
    value_list _allocas{};
    for (auto it = list.begin(); it != list.end();) {
        if (auto alloca_ = dynamic_cast<Instruction::alloca_ *>(*it)) {
            _allocas.push_back(alloca_);
            it = list.erase(it);
        } else
            ++it;
    }
    _allocas.splice(_allocas.end(), list);
    list = _allocas;

    bool is_terminator = true;
    mir::BasicBlock *cur = nullptr;
    for (auto item : list) {
        if (auto bb = dynamic_cast<mir::BasicBlock *>(item)) {
            if (!is_terminator) {
                auto br = new Instruction::br(bb);
                assert(cur);
                cur->push_back(br);
            }
            cur = bb;
            current_function->bbs.push_back(bb);
            is_terminator = false;
        } else {
            if (is_terminator) {
                cur = new mir::BasicBlock(current_function);
                current_function->bbs.push_back(cur);
            }
            auto instr = dynamic_cast<Instruction *>(item);
            assert(cur && instr);
            cur->push_back(instr);
            is_terminator = instr->isTerminator();
        }
    }

    if (cur == nullptr) {
        cur = new mir::BasicBlock(current_function);
        current_function->bbs.push_back(cur);
        cur->push_back(Instruction::ret::default_t(current_function->retType));
    } else if (cur->instructions.empty() || !cur->instructions.back()->isTerminator()) {
        cur->push_back(Instruction::ret::default_t(current_function->retType));
    }
}
}  // namespace frontend::visitor

// visitor: specific methods
namespace frontend::visitor {
template <>
SysYVisitor::return_type SysYVisitor::visit<CompUnit>(const GrammarNode &node) {
    for (auto func : mir::Function::getLibrary()) {
        auto name = std::string_view(func->name).substr(1);
        if (auto idx = name.find_last_of('_'); idx != std::string_view::npos)
            name = name.substr(idx + 1);
        auto msg = symbol_table.insert(name, {func, nullptr}, Token());
        assert(msg == std::nullopt);
    }
    return visitChildren(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<BType>(const frontend::grammar::GrammarNode &node) {
    if (node.children[0]->getToken().type == INTTK) {
        current_btype = mir::Type::getI32Type();
    } else if (node.children[0]->getToken().type == FLOATTK) {
        current_btype = mir::Type::getFloatType();
    } else {
        __builtin_unreachable();
    }
    return {};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ConstDef>(const GrammarNode &node) {
    // IDENFR (LBRACK constExp RBRACK)* ASSIGN constInitVal
    auto &identifier = node.children[0]->getToken();
    auto type = getVarType(node.children.begin() + 1, node.children.end() - 2);
    auto [value, list] = visit(*node.children.back());
    auto literal = dynamic_cast<mir::Literal *>(value);
    assert(literal && list.empty());
    if (type->isNumberTy()) {
        value_list _l{};
        auto _r = convert_to(literal, type, true, _l);
        literal = dynamic_cast<mir::Literal *>(_r);
        assert(literal && _l.empty());
    } else {
        auto [_r, _l] = array_cast(dynamic_cast<mir::ArrayLiteral *>(literal), type);
        mir::try_delete(literal);
        literal = dynamic_cast<mir::Literal *>(_r);
        assert(literal && _l.empty());
    }
    // always global
    mir::GlobalVar *variable;
    if (current_function) {
        variable = new mir::GlobalVar(type, literal, true);
        variable->name =
            "@__const." + current_function->name.substr(1) + "." + std::string(identifier.raw);
    } else {
        variable = new mir::GlobalVar(type, std::string(identifier.raw), literal, true);
    }
    manager.globalVars.push_back(variable);
    if (auto msg = symbol_table.insert(identifier.raw, {variable, literal}, identifier)) {
        message_queue.push_back(*msg);
    }
    return {};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ConstExp>(const GrammarNode &node);

template <>
SysYVisitor::return_type SysYVisitor::visit<ConstInitVal>(const GrammarNode &node) {
    // constExp | LBRACE (constInitVal (COMMA constInitVal)*) RBRACE
    if (node.children.size() == 1) return visit<ConstExp>(*node.children[0]);
    if (node.children.size() == 2) return {new mir::ArrayLiteral({}), {}};
    std::vector<mir::Literal *> literals;
    for (auto &child : node.children) {
        if (child->type == Terminal) continue;
        auto [value, list] = visit<ConstInitVal>(*child);
        auto literal = dynamic_cast<mir::Literal *>(value);
        assert(literal && list.empty());
        literals.push_back(literal);
    }
    return {new mir::ArrayLiteral(std::move(literals)), {}};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<VarDef>(const GrammarNode &node) {
    // IDENFR (LBRACK constExp RBRACK)* (ASSIGN initVal)?
    auto &identifier = node.children[0]->getToken();
    auto &initVal = node.children.back()->type == InitVal ? node.children.back() : nullptr_node;
    auto type = getVarType(node.children.begin() + 1, node.children.end() - (initVal ? 2 : 0));
    if (current_function) {
        auto alloca_ = new Instruction::alloca_(type);
        if (auto msg = symbol_table.insert(identifier.raw, {alloca_, nullptr}, identifier)) {
            message_queue.push_back(*msg);
        }
        if (initVal == nullptr) return {nullptr, {alloca_}};
        auto [val, list] = visit(*initVal);
        list.push_back(alloca_);
        list.splice(list.end(), storeInitValue(alloca_, type, val));
        mir::try_delete(val);
        return {nullptr, list};
    }
    mir::Literal *literal;
    if (initVal) {
        auto [value, list] = visit<ConstInitVal>(*initVal);
        literal = dynamic_cast<mir::Literal *>(value);
        assert(literal && list.empty());
        if (auto arr = dynamic_cast<mir::ArrayLiteral *>(literal)) {
            auto [v, l] = array_cast(arr, type);
            mir::try_delete(arr);
            literal = dynamic_cast<mir::Literal *>(v);
            assert(literal && l.empty());
        }
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

template <>
SysYVisitor::return_type SysYVisitor::visit<InitVal>(const GrammarNode &node) {
    // exp | LBRACE (initVal (COMMA initVal)*)? RBRACE
    if (node.children.size() == 1) return visit<Exp>(*node.children.front());
    if (node.children.size() == 2) return {new mir::ArrayValue({}), {}};
    value_vector values{};
    value_list list{};
    for (auto &child : node.children) {
        if (child->type == Terminal) continue;
        auto [v, l] = visit<InitVal>(*child);
        list.splice(list.end(), l);
        values.push_back(v);
    }
    if (std::all_of(values.begin(), values.end(), [](auto val) {
            return val == mir::getIntegerLiteral(0) || val == mir::getFloatLiteral(0.0f);
        })) {
        return {new mir::ArrayValue({}), list};
    }
    return {new mir::ArrayValue(std::move(values)), list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<FuncDef>(const GrammarNode &node) {
    // funcType IDENFR LPARENT funcFParams? RPARENT block
    auto &retType = node.children.front()->children.front()->getToken().type;
    auto funcRetType = retType == VOIDTK    ? mir::Type::getVoidType()
                       : retType == FLOATTK ? mir::Type::getFloatType()
                                            : mir::Type::getI32Type();
    auto &identifier = node.children[1]->getToken();
    decltype(token_buffer)().swap(token_buffer);
    value_list params_list =
        node.children.size() == 6 ? visit(*node.children[3]).second : value_list{};
    std::vector<mir::pType> params;
    std::transform(params_list.begin(), params_list.end(), std::back_inserter(params),
                   [](value_type value) { return value->type; });
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
        auto arg = func->addArgument((*it1)->type);
        mir::Value *val = arg;
        if (arg->type->isNumberTy()) {
            auto alloca_ = new Instruction::alloca_(arg->type);
            auto store_ = new Instruction::store(arg, alloca_);
            init_list.push_back(alloca_);
            init_list.push_back(store_);
            val = alloca_;
        }
        if (auto msg = symbol_table.insert((*it1)->name, {val, nullptr}, **it2)) {
            message_queue.push_back(*msg);
        }
    }
    auto [_, list] = visit(*node.children.back());
    init_list.splice(init_list.end(), list);
    listToBB(init_list, node.children.back()->children.back()->getToken());
    current_function = nullptr;
    return {};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<FuncFParam>(const GrammarNode &node) {
    // bType IDENFR (LBRACK RBRACK (LBRACK constExp RBRACK)*)?
    visit(*node.children[0]);
    auto &identifier = node.children[1]->getToken();
    auto type = getVarType(node.children.begin() + 2, node.children.end());
    auto virtual_value = new mir::Value(type);
    virtual_value->name = std::string(identifier.raw);
    token_buffer.push_back(&identifier);
    return {nullptr, {virtual_value}};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<Block>(const GrammarNode &node) {
    // LBRACE blockItem* RBRACE
    symbol_table.enter_block();
    auto result = visitChildren(node);
    symbol_table.exit_block();
    return result;
}

template <>
SysYVisitor::return_type SysYVisitor::visit<IfStmt>(const GrammarNode &node) {
    // IFTK LPARENT cond RPARENT stmt (ELSETK stmt)?
    cond_stack.push({new mir::BasicBlock(current_function), new mir::BasicBlock(current_function)});
    auto [cond_v, cond_l] = visit(*node.children[2]);
    auto [if_v, if_l] = visit(*node.children[4]);
    value_list list = {};
    list.splice(list.end(), cond_l);
    list.push_back(cond_stack.top().true_block);
    list.splice(list.end(), if_l);
    if (node.children.size() == 7) {
        auto [else_v, else_l] = visit(*node.children[6]);
        auto end_block = new mir::BasicBlock(current_function);
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

template <>
SysYVisitor::return_type SysYVisitor::visit<WhileStmt>(const GrammarNode &node) {
    // WHILETK LPARENT cond RPARENT stmt
    auto &cond = node.children[2];
    auto &stmt = node.children[4];
    // (continue) -> cond -> (true) -> stmt -> (br) -> (break/false)
    loop_stack.push({new mir::BasicBlock(current_function), new mir::BasicBlock(current_function)});
    cond_stack.push({new mir::BasicBlock(current_function), loop_stack.top().break_block});
    value_list list{};
    list.push_back(new Instruction::br(loop_stack.top().continue_block));
    list.push_back(loop_stack.top().continue_block);
    auto [cond_v, cond_l] = visit(*cond);
    list.splice(list.end(), cond_l);
    list.push_back(cond_stack.top().true_block);
    auto [stmt_v, stmt_l] = visit(*stmt);
    list.splice(list.end(), stmt_l);
    list.push_back(new Instruction::br(loop_stack.top().continue_block));
    list.push_back(cond_stack.top().false_block);
    cond_stack.pop();
    loop_stack.pop();
    return {nullptr, list};
}

template <>
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
    auto &forStmt1 = forStmt1_it == semicn1 ? nullptr_node : *forStmt1_it;
    auto &cond = cond_it == semicn2 ? nullptr_node : *cond_it;
    auto &forStmt2 = forStmt2_it == node.children.end() ? nullptr_node : *forStmt2_it;
    auto &stmt = node.children.back();
    // forStmt1 -> (cond) -> cond -> (true) -> stmt -> (continue) -> forStmt2 -> (br) ->
    // (break/false)
    auto cond_block = new mir::BasicBlock(current_function);
    loop_stack.push({new mir::BasicBlock(current_function), new mir::BasicBlock(current_function)});
    cond_stack.push({new mir::BasicBlock(current_function), loop_stack.top().break_block});
    value_list list = {};
    if (forStmt1) {
        auto [v, l] = visit(*forStmt1);
        list.splice(list.end(), l);
    }
    list.push_back(new Instruction::br(cond_block));
    list.push_back(cond_block);
    if (cond) {
        auto [v, l] = visit(*cond);
        list.splice(list.end(), l);
    } else {
        auto br = new Instruction::br(cond_stack.top().true_block);
        list.push_back(br);
    }
    list.push_back(cond_stack.top().true_block);
    auto [_v, _l] = visit(*stmt);
    list.splice(list.end(), _l);
    list.push_back(loop_stack.top().continue_block);
    if (forStmt2) {
        auto [v, l] = visit(*forStmt2);
        list.splice(list.end(), l);
    }
    list.push_back(new Instruction::br(cond_block));
    list.push_back(cond_stack.top().false_block);
    cond_stack.pop();
    loop_stack.pop();
    return {nullptr, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<AssignStmt>(const GrammarNode &node) {
    // lVal ASSIGN exp SEMICN
    auto [variable, list] = visit(*node.children[0]);
    auto [value, l] = visit(*node.children[2]);
    list.splice(list.end(), l);
    if (variable->isConstLVal()) {
        auto &[ty, raw, line, column] = node.children[0]->children[0]->getToken();
        message_queue.push_back(
            message{message::ERROR, 'h', line, column, "cannot assign to const variable"});
        return {nullptr, list};
    }
    auto store = new Instruction::store(convert_to(value, variable->type, true, list), variable);
    list.push_back(store);
    return {store, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<BreakStmt>(const GrammarNode &node) {
    // BREAKTK SEMICN
    if (loop_stack.empty()) {
        auto &[ty, raw, line, column] = node.children[0]->getToken();
        message_queue.push_back(
            message{message::ERROR, 'm', line, column, "break statement not within a loop"});
        return {nullptr, {}};
    }
    return {nullptr, {new Instruction::br(loop_stack.top().break_block)}};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ContinueStmt>(const GrammarNode &node) {
    // CONTINUETK SEMICN
    if (loop_stack.empty()) {
        auto &[ty, raw, line, column] = node.children[0]->getToken();
        message_queue.push_back(
            message{message::ERROR, 'm', line, column, "continue statement not within a loop"});
        return {nullptr, {}};
    }
    return {nullptr, {new Instruction::br(loop_stack.top().continue_block)}};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ReturnStmt>(const GrammarNode &node) {
    // RETURNTK exp? SEMICN
    auto &[ty, raw, line, column] = node.children[0]->getToken();
    if (current_function->retType == mir::Type::getVoidType() && node.children.size() == 3) {
        message_queue.push_back(
            message{message::ERROR, 'f', line, column, "void function should not return a value"});
        return {nullptr, {}};
    }
    if (current_function->retType != mir::Type::getVoidType() && node.children.size() == 2) {
        message_queue.push_back(
            message{message::ERROR, 'f', line, column, "Non-void function should return a value"});
        return {nullptr, {}};
    }
    if (node.children.size() == 2) return {nullptr, {new Instruction::ret()}};
    auto [value, list] = visit(*node.children[1]);
    list.push_back(new Instruction::ret(convert_to(value, current_function->retType, true, list)));
    return {nullptr, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ForStmt>(const GrammarNode &node) {
    return visit<AssignStmt>(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<LVal>(const GrammarNode &node) {
    // IDENFR (LBRACK exp RBRACK)*
    auto &[_, raw, line, column] = node.children[0]->getToken();
    auto [variable, literal] = symbol_table.lookup(raw);
    if (variable == nullptr) {
        message_queue.push_back(message{message::ERROR, 'c', line, column,
                                        "undefined symbol '" + std::string(raw) + "'"});
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
    }
    value_list list = {};
    if (node.children.size() > 1) {
        list.splice(list.end(), l);
        auto ty = variable->type;
        for (int i = 1; i < indices.size(); i++) ty = ty->getBase();
        variable = new Instruction::getelementptr(ty, variable, indices);
        list.push_back(variable);
    }
    return {variable, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<PrimaryExp>(const GrammarNode &node) {
    // LPARENT exp RPARENT | lVal | number
    if (in_const_expr) return visitChildren(node);
    auto [value, list] = visitChildren(node);
    if (node.children[0]->type == LVal && value->type->isNumberTy()) {
        value = new Instruction::load(value->type, value);
        list.push_back(value);
    }
    return {value, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<Number>(const GrammarNode &node) {
    // INTCON
    auto &token = node.children[0]->getToken();
    auto &token_raw = token.raw;
    if (token.type == INTCON) {
        auto value = std::stoi(std::string{token_raw}, nullptr, 0);
        auto literal = mir::getIntegerLiteral(value);
        return {literal, {}};
    } else {
        auto value = std::stof(std::string{token_raw});
        auto literal = mir::getFloatLiteral(value);
        return {literal, {}};
    }
}

template <>
SysYVisitor::return_type SysYVisitor::visit<UnaryExp>(const GrammarNode &node) {
    // primaryExp | IDENFR LPARENT funcRParams? RPARENT | unaryOp unaryExp
    if (node.children[0]->type == PrimaryExp) return visit(*node.children[0]);
    if (node.children[0]->type == Terminal) {
        auto &[_, raw, line, column] = node.children[0]->getToken();
        auto variable = symbol_table.lookup(raw).first;
        if (variable == nullptr) {
            message_queue.push_back(message{message::ERROR, 'c', line, column,
                                            "undefined symbol '" + std::string(raw) + "'"});
            return {zero_value, {}};
        }
        auto function = dynamic_cast<mir::Function *>(variable);
        if (function == nullptr) {
            message_queue.push_back(message{message::ERROR, 'c', line, column,
                                            "symbol '" + std::string(raw) + "' is not a function"});
            return {zero_value, {}};
        }
        value_list list = {};
        if (node.children.size() == 3) {
            if (function == mir::Function::starttime() || function == mir::Function::stoptime()) {
                auto call = new Instruction::call(function, {mir::getLiteral((int)line)});
                list.push_back(call);
                return {call, list};
            }
            if (function->type->getFunctionParamCount() != 0)
                message_queue.push_back(
                    message{message::ERROR, 'd', line, column,
                            "too few arguments for function '" + std::string(raw) + "'"});
            auto call = new Instruction::call(function, {});
            list.push_back(call);
            return {call, list};
        }
        // visit funcRParams
        auto [value, l] =
            visitExps(node.children[2]->children.begin(), node.children[2]->children.end());
        list.splice(list.end(), l);
        if (function->type->getFunctionParamCount() != value.size()) {
            message_queue.push_back(message{
                message::ERROR, 'd', line, column,
                std::string("too ") +
                    (function->type->getFunctionParamCount() < value.size() ? "many" : "few") +
                    " arguments for function '" + std::string(raw) + "'"});
        } else {
            for (int i = 0; i < value.size(); i++) {
                if (!value[i]->type->convertableTo(function->type->getFunctionParam(i))) {
                    message_queue.push_back(
                        message{message::ERROR, 'e', line, column,
                                std::string("incompatible type for the ") + std::to_string(i + 1) +
                                    "th argument of function '" + std::string(raw) + "'"});
                }
                value[i] = convert_to(value[i], function->type->getFunctionParam(i), true, list);
            }
        }
        auto call = new Instruction::call(function, value);
        list.push_back(call);
        return {call, list};
    }
    auto [value, list] = visit(*node.children[1]);
    auto unary_op_type = node.children[0]->children[0]->getToken().type;
    if (in_const_expr) {
        auto literal = dynamic_cast<mir::Literal *>(value);
        assert(literal && list.empty());
        if (unary_op_type == MINU) {
            literal = std::visit([](auto v) { return mir::getLiteral(-v); }, literal->getValue());
        } else {
            assert(unary_op_type == PLUS);
        }
        return {literal, {}};
    }
    switch (unary_op_type) {
    case PLUS: break;
    case MINU:
        if (value->type->isIntegerTy())
            value = new Instruction::sub(zero_value,
                                         convert_to(value, mir::Type::getI32Type(), false, list));
        else
            value = new Instruction::fneg(value);
        list.push_back(value);
        break;
    case NOT: value = convert_to_bool(value, true, list); break;
    default: assert(false);
    }
    return {value, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<MulExp>(const GrammarNode &node) {
    // unaryExp ((MULT | DIV | MOD) unaryExp)*
    return visitBinaryExp(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<AddExp>(const GrammarNode &node) {
    // mulExp ((PLUS | MINU) mulExp)*
    return visitBinaryExp(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<RelExp>(const GrammarNode &node) {
    // addExp ((LSS | LEQ | GRE | GEQ) addExp)*
    return visitBinaryExp(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<EqExp>(const GrammarNode &node) {
    // relExp ((EQL | NEQ) relExp)*
    return visitBinaryExp(node);
}

template <>
SysYVisitor::return_type SysYVisitor::visit<LAndExp>(const GrammarNode &node) {
    // eqExp (AND eqExp)*
    auto [value, list] = visit(*node.children[0]);
    value = convert_to_bool(value, false, list);
    for (auto it = node.children.begin() + 1; it != node.children.end(); ++it) {
        if ((*it)->type == Terminal) continue;
        auto bb = new mir::BasicBlock(current_function);
        list.push_back(new Instruction::br(value, bb, cond_stack.top().false_block));
        list.push_back(bb);
        auto [v, l] = visit(**it);
        value = convert_to_bool(v, false, l);
        list.splice(list.end(), l);
    }
    list.push_back(
        new Instruction::br(value, cond_stack.top().true_block, cond_stack.top().false_block));
    return {value, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<LOrExp>(const GrammarNode &node) {
    // lAndExp (OR lAndExp)*
    auto count =
        std::count_if(node.children.begin(), node.children.end(), &is_specific_type<LAndExp>);
    value_type value = nullptr;
    value_list list = {};
    for (int i = 0; i < count; i++) {
        auto &lAndExp = *node.children[i * 2];
        mir::BasicBlock *false_block =
            i + 1 == count ? cond_stack.top().false_block : new mir::BasicBlock(current_function);
        cond_stack.push({cond_stack.top().true_block, false_block});
        auto [v, l] = visit(lAndExp);
        value = convert_to_bool(v, false, l);
        list.splice(list.end(), l);
        if (i + 1 != count) list.push_back(false_block);
        cond_stack.pop();
    }
    return {value, list};
}

template <>
SysYVisitor::return_type SysYVisitor::visit<ConstExp>(const GrammarNode &node) {
    // addExp
    in_const_expr = true;
    auto [value, list] = visit(*node.children[0]);
    in_const_expr = false;
    return {value, list};
}
}  // namespace frontend::visitor

// visitor: default methods
namespace frontend::visitor {
SysYVisitor::return_type SysYVisitor::visit(const GrammarNode &node) {
    return (this->*visitor_methods[node.type])(node);
}

SysYVisitor::return_type SysYVisitor::visitChildren(const GrammarNode &node) {
    value_type value{};
    value_list list{};
    for (const auto &ptr : node.children) {
        auto [r1, r2] = visit(*ptr);
        value = r1 ? r1 : value;
        list.splice(list.end(), r2);
    }
    return {value, list};
}

// default: do nothing
template <grammar_type_t>
SysYVisitor::return_type SysYVisitor::visit(const GrammarNode &node) {
    return visitChildren(node);
}
}  // namespace frontend::visitor
