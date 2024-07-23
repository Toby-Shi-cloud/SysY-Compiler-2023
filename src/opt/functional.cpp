//
// Created by toby on 2023/12/1.
//

#include "opt.h"

namespace mir {
void functionInline(Function *func) {
    for (auto bb_it = func->bbs.begin(); bb_it != func->bbs.end(); ++bb_it) {
        auto &&bb = *bb_it;
        for (auto inst_it = bb->instructions.begin(); inst_it != bb->instructions.end();
             ++inst_it) {
            auto &&call = dynamic_cast<Instruction::call *>(*inst_it);
            if (!call || call->getFunction()->isRecursive() || call->getFunction()->isLibrary())
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
        if (!arg->type->isIntegerTy()) func->isPure = false;
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
}  // namespace mir
