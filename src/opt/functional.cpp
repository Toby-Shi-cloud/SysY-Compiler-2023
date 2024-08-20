//
// Created by toby on 2023/12/1.
//

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "mir/derived_value.h"
#include "mir/manager.h"
#include "mir/type.h"
#include "opt/mem2reg.h"
#include "opt/opt.h"

namespace mir {
void functionInline(Function *func) {
    for (auto bb_it = func->bbs.begin(); bb_it != func->bbs.end(); ++bb_it) {
        auto &&bb = *bb_it;
        for (auto inst_it = bb->instructions.begin(); inst_it != bb->instructions.end();
             ++inst_it) {
            auto &&call = dynamic_cast<Instruction::call *>(*inst_it);
            if (!call || call->getFunction()->isRecursive() || call->getFunction()->isLibrary())
                continue;
            // 一些奇怪的不 inline 的条件
            auto call_ops = call->getOperands();
            if (call->getFunction()->instruction_size() > 200 &&
                std::all_of(call_ops.begin() + 1, call_ops.end(),
                            [](Value *arg) { return dynamic_cast<Literal *>(arg) == nullptr; }))
                continue;
            // 1. clone function & replace args
            auto callee = call->getFunction()->clone();
            for (int i = 0; i < call->getNumArgs(); i++) callee->args[i]->moveTo(call->getArg(i));
            // 2. prepare return bb
            auto ret_bb = new BasicBlock(func);
            auto nxt_inst_it = inst_it;
            ret_bb->splice(ret_bb->instructions.cend(), bb, ++nxt_inst_it, bb->instructions.cend());
            auto nxt_bb_it = bb_it;
            nxt_bb_it = func->bbs.insert(++nxt_bb_it, ret_bb);
            // 3. prepare return value
            auto ret_val = call->isValue() ? new Instruction::phi(call->type) : nullptr;
            if (ret_val) ret_bb->push_front(ret_val);
            auto call_br = new Instruction::br(callee->bbs.front());
            if (ret_val)
                substitute(call, call_br, static_cast<Value *>(ret_val));
            else
                substitute(call, call_br);
            // 4. substitute ret inst
            for (auto &&callee_bb : callee->bbs)
                if (auto ret = dynamic_cast<Instruction::ret *>(callee_bb->instructions.back())) {
                    auto val = ret->getReturnValue();
                    substitute(ret, new Instruction::br(ret_bb));
                    if (val && ret_val) ret_val->addIncomingValue({val, callee_bb});
                }
            // 5. move alloca
            auto start_bb = func->bbs.front();
            auto callee_start_bb = callee->bbs.front();
            start_bb->splice(start_bb->beginner_end(), callee_start_bb,
                             callee_start_bb->instructions.cbegin(),
                             callee_start_bb->beginner_end());
            // 6. splice bbs
            func->splice(nxt_bb_it, callee, callee->bbs.cbegin(), callee->bbs.cend());
            if (ret_val) constantFolding(ret_val);
            // 7. delete callee
            delete callee;
            opt_infos.function_inline()++;
            // 8. move phi(bb) -> phi(ret_bb)
            bb->moveTo(ret_bb, [](auto &&user) {
                return dynamic_cast<Instruction::phi *>(user) != nullptr;
            });
            break;
        }
    }
}

void connectBlocks(Function *func) {
    func->calcPreSuc();
    func->markBBNode();
    for (auto it = func->bbs.begin(); it != func->bbs.end(); ++it) {
        auto &&bb = *it;
        if (bb->successors.size() != 1) continue;
        auto suc = *bb->successors.begin();
        if (suc == func->exitBB || suc->predecessors.size() != 1) continue;
        assert(suc->predecessors.count(bb));
        assert(suc != bb);
        for (auto it2 = suc->instructions.begin(); it2 != suc->phi_end();)
            it2 = constantFolding(*it2);
        bb->pop_back();
        bb->splice(bb->instructions.cend(), suc, suc->instructions.cbegin(),
                   suc->instructions.cend());
        func->bbs.erase(suc->node);
        suc->moveTo(bb);
        delete suc;
        func->calcPreSuc();
        opt_infos.merge_empty_block()++;
    }
}

void calcPure(Function *func) {
    if (func->isLibrary()) return;
    func->isPure = true;
    func->noPostEffect = true;
    for (auto &&arg : func->args)
        if (!arg->type->isIntegerTy() && !arg->type->isFloatTy()) func->isPure = false;
    auto setAttr = [&func](auto &&inst) {
        auto rt = getRootValue(inst).first;
        if (dynamic_cast<const GlobalVar *>(rt)) func->noPostEffect = false;
        if (dynamic_cast<const Argument *>(rt)) func->noPostEffect = false;
    };
    for (auto bb : func->bbs)
        for (auto inst : bb->instructions) {
            if (auto call = dynamic_cast<Instruction::call *>(inst)) {
                func->isPure &= call->getFunction()->isPure;
                func->noPostEffect &= call->getFunction()->noPostEffect;
            }
            for (auto &&op : inst->getOperands())
                if (dynamic_cast<GlobalVar *>(op)) func->isPure = false;
            if (auto store = dynamic_cast<Instruction::store *>(inst)) setAttr(store);
            if (auto memset = dynamic_cast<Instruction::memset *>(inst)) setAttr(memset);
            if (auto call = dynamic_cast<Instruction::call *>(inst);
                call && !call->getFunction()->noPostEffect)
                for (auto i = 0; i < call->getNumArgs(); i++) setAttr(call->getArg(i));
            if (!func->noPostEffect) return;
        }
}

calculate_t Function::interpret(const std::vector<calculate_t> &_args_v) const {
    assert(isPure);
    Interpreter interpreter;
    for (int i = 0; i < args.size(); i++) interpreter.map[args[i]] = _args_v[i];
    interpreter.currentBB = bbs.front();
    while (interpreter.currentBB) {
        for (auto &&inst : interpreter.currentBB->instructions) {
            if (inst->node == interpreter.currentBB->beginner_end()) {
                for (auto &&[k, v] : interpreter.phi) interpreter.map[k] = v;
                interpreter.phi.clear();
            }
            inst->interpret(interpreter);
        }
    }
    return interpreter.retValue;
}

// get first block except alloca
BasicBlock *splitAndGetFront(Function *func) {
    auto begin_bb = func->bbs.front();
    auto alloca_end = begin_bb->beginner_end();
    if (std::next(alloca_end) == begin_bb->instructions.end()) {
        auto br = dynamic_cast<Instruction::br *>(*alloca_end);
        if (br && !br->hasCondition()) return br->getTarget();
    }
    // should split (no phi)
    auto bb = new BasicBlock(func);
    func->bbs.insert(std::next(begin_bb->node), bb);
    bb->splice(bb->instructions.begin(), begin_bb, alloca_end, begin_bb->instructions.end());
    begin_bb->push_back(new Instruction::br(bb));
    return bb;
}

void trailRecursionOpt(Function *func) {
    std::vector<std::tuple<Instruction::ret *, Instruction::call *>> candidates;
    for (auto block : func->bbs) {
        if (block->instructions.size() < 2) continue;
        auto it = block->instructions.end();
        auto ret = dynamic_cast<Instruction::ret *>(*--it);
        if (!ret) continue;
        auto call = dynamic_cast<Instruction::call *>(*--it);
        if (!call || call->getFunction() != func) continue;
        if (!func->retType->isVoidTy() && ret->getReturnValue() != call) continue;
        // could optimize...
        candidates.emplace_back(ret, call);
    }
    if (candidates.empty()) return;

    opt_infos.trail_recursion() += (int)candidates.size();
    auto begin_bb = splitAndGetFront(func);
    std::vector<std::pair<Value *, Instruction::phi *>> arg2phi;
    for (auto it = begin_bb->instructions.begin(); it != begin_bb->phi_end(); ++it) {
        auto phi = dynamic_cast<Instruction::phi *>(*it);
        auto value = phi->getIncomingValue(func->bbs.front());
        arg2phi.emplace_back(value.first, phi);
    }
    for (auto [ret, call] : candidates) {
        auto block = ret->parent;
        std::unordered_map<Value *, Value *> arg2val;
        // 先删掉，之后加回来
        for (auto [_, phi] : arg2phi) phi->eraseIncomingValue(func->bbs.front());
        for (auto arg : func->args) {
            if (!arg->isUsed()) continue;
            auto phi = new Instruction::phi(arg->type);
            arg2phi.emplace_back(arg, phi);
            begin_bb->insert(begin_bb->phi_end(), phi);
            arg->moveTo(phi);
        }
        for (int i = 0; i < call->getNumArgs(); i++) {
            auto arg = func->args[i];
            auto val = call->getArg(i);
            arg2val.emplace(arg, val);
        }
        for (auto &[val, phi] : arg2phi) {
            phi->addIncomingValue({val, func->bbs.front()});
            if (auto it = arg2val.find(val); it != arg2val.end()) {
                phi->addIncomingValue({it->second, block});
            } else {
                phi->addIncomingValue({val, block});
            }
        }
        block->erase(ret);
        block->erase(call);
        block->push_back(new Instruction::br(begin_bb));
    }
}

void usingX64(Function *func, Manager &manage) {
    calcPure(func);
    if (!func->isRecursive() || !func->isPure || func->retType != Type::getI32Type()) return;
    constexpr auto get_mod = [](Function *func) -> Value * {
        Value *mod = nullptr;
        for (auto bb : func->bbs) {
            auto ret = dynamic_cast<Instruction::ret *>(bb->instructions.back());
            if (!ret) continue;
            auto val = ret->getReturnValue();
            if (dynamic_cast<IntegerLiteral *>(val) || dynamic_cast<Argument *>(val)) {
                // ok
            } else if (auto r = dynamic_cast<Instruction::srem *>(val)) {
                if (mod == nullptr)
                    mod = r->getRhs();
                else if (mod != r->getRhs())
                    return nullptr;
            } else {
                return nullptr;
            }
        }
        return mod;
    };
    if (get_mod(func) == nullptr) return;
    // func could be translate to x64 ver
    auto x64_func = func->clone();
    x64_func->name = func->name + ".x64";
    auto mod = get_mod(x64_func);
    std::unordered_map<Value *, Value *> mapped;  // x32 -> x64
    // change args
    for (auto &arg : x64_func->args)
        if (arg->type == Type::getI32Type()) {
            auto _new = new Argument(Type::getI64Type(), x64_func);
            auto bb = x64_func->bbs.front();
            auto _trunc = new Instruction::trunc(Type::getI32Type(), _new);
            arg->moveTo(_trunc);
            bb->insert(bb->beginner_end(), _trunc);
            delete arg;
            arg = _new;
            mapped[_trunc] = _new;
        }
    x64_func->retType = Type::getI64Type();
    std::vector<pType> arg_ty;
    arg_ty.reserve(x64_func->args.size());
    for (auto arg : x64_func->args) arg_ty.push_back(arg->type);
    x64_func->type = FunctionType::getFunctionType(x64_func->retType, std::move(arg_ty));
    // dfs
    const auto dfs = [&](Value *value, Instruction *fa, auto &&self) -> Value * {
        if (value->type == Type::getI64Type()) return value;
        assert(value->type == Type::getI32Type());
        auto &result = mapped[value];
        if (result) {
            return result;
        } else if (auto rem = dynamic_cast<Instruction::srem *>(value)) {
            auto lhs = self(rem->getLhs(), rem, self), rhs = self(rem->getRhs(), rem, self);
            if (rhs == mod) {
                return result = lhs;
            } else {
                auto _new = new Instruction::srem(lhs, rhs);
                rem->parent->insert(rem->node, _new);
                return result = _new;
            }
        } else if (auto inst = dynamic_cast<Instruction *>(value);
                   inst && inst->instrTy >= Instruction::ADD &&
                   inst->instrTy <= Instruction::SREM) {
            auto lhs = self(inst->getOperand(0), inst, self);
            auto rhs = self(inst->getOperand(1), inst, self);
            auto _new = inst->clone();
            _new->substituteOperand(inst->getOperand(0), lhs);
            _new->substituteOperand(inst->getOperand(1), rhs);
            _new->type = Type::getI64Type();
            inst->parent->insert(inst->node, _new);
            return result = _new;

        } else if (auto lit = dynamic_cast<IntegerLiteral *>(value)) {
            return result = Int64Literal::get(lit->value);
        } else if (auto call = dynamic_cast<Instruction::call *>(value);
                   call && call->getFunction() == func) {
            std::vector<Value *> args;
            args.reserve(call->getNumArgs());
            for (int i = 0; i < call->getNumArgs(); i++) {
                auto arg = call->getArg(i);
                if (arg->type == Type::getI32Type())
                    args.push_back(self(arg, call, self));
                else
                    args.push_back(arg);
            }
            auto _ncall = new Instruction::call(x64_func, args);
            call->parent->insert(call->node, _ncall);
            return result = _ncall;
        }
        auto _new = new Instruction::sext(Type::getI64Type(), value);
        fa->parent->insert(fa->node, _new);
        return result = _new;
    };
    // mod
    mod = dfs(mod, dynamic_cast<Instruction *>(mod), dfs);
    for (auto bb : x64_func->bbs) {
        if (auto ret = dynamic_cast<Instruction::ret *>(bb->instructions.back())) {
            auto _new = dfs(ret->getReturnValue(), ret, dfs);
            ret->substituteOperand(ret->getReturnValue(), _new);
        } else if (auto br = dynamic_cast<Instruction::br *>(bb->instructions.back());
                   br && br->hasCondition())
            if (auto icmp = dynamic_cast<Instruction::icmp *>(br->getCondition())) {
                auto lhs = dfs(icmp->getLhs(), icmp, dfs);
                auto rhs = dfs(icmp->getRhs(), icmp, dfs);
                icmp->substituteOperand(icmp->getLhs(), lhs);
                icmp->substituteOperand(icmp->getRhs(), rhs);
            }
    }
    clearDeadInst(x64_func);
    clearDeadBlock(x64_func);
    dbg(*x64_func);
    manage.functions.push_back(nullptr);
    for (auto i = manage.functions.size() - 1;; --i) {
        if (manage.functions[i] == func) {
            manage.functions[i] = x64_func;
            break;
        } else {
            manage.functions[i] = manage.functions[i - 1];
        }
    }
    {
        for (auto &bb : func->bbs) delete bb;
        func->bbs.clear();
        func->bbs.push_back(new BasicBlock(func));
        auto bb = func->bbs.front();
        std::vector<Value *> args;
        args.reserve(func->args.size());
        for (auto arg : func->args) {
            if (arg->type == Type::getI32Type()) {
                auto sext = new Instruction::sext(Type::getI64Type(), arg);
                args.push_back(sext);
                bb->push_back(sext);
            } else {
                args.push_back(arg);
            }
        }
        auto call = new Instruction::call(x64_func, args);
        bb->push_back(call);
        auto rem = new Instruction::srem(call, mod);
        bb->push_back(rem);
        auto trunc = new Instruction::trunc(Type::getI32Type(), rem);
        bb->push_back(trunc);
        auto ret = new Instruction::ret(trunc);
        bb->push_back(ret);
    }
}
}  // namespace mir
