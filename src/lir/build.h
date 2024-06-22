//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_LIR_BUILD_H
#define COMPILER_LIR_BUILD_H

#include <unordered_map>
#include "dag.h"

namespace LIR {
    using value_map_t = std::unordered_map<mir::Value *, DAGValue *>;

    struct CallArg {
        DAG dag;
        value_map_t map;
        DAGValue *last_side_effect = nullptr;
    };

    inline auto build_constant_node(mir::Literal *literal, value_map_t &map) {
        auto node = std::make_unique<node::Constant>(literal);
        map[literal] = &node->value;
        return node;
    }

    inline auto build_vreg_node(mir::Value *value, CallArg &arg) {
        auto reg = std::make_unique<node::VRegister>(value->getId(), value->type);
        auto node = std::make_unique<node::CopyFromReg>(reg->value.type);
        node->reg.set_link(&reg->value);
        node->dependency.set_link(arg.map[nullptr]);
        arg.map[value] = &node->value;
        arg.dag.push_back(std::move(reg));
        return node;
    }

    inline auto build_gvar_node(mir::GlobalVar *gvar, value_map_t &map) {
        auto node = std::make_unique<node::GAddress>(gvar);
        map[gvar] = &node->value;
        return node;
    }

    inline auto get_node_value(mir::Value *value, CallArg &arg) {
        if (auto val = arg.map[value])
            return val;
        if (auto lit = dynamic_cast<mir::Literal *>(value)) {
            arg.dag.push_back(build_constant_node(lit, arg.map));
        } else if (auto gvar = dynamic_cast<mir::GlobalVar *>(value)) {
            arg.dag.push_back(build_gvar_node(gvar, arg.map));
        } else {
            arg.dag.push_back(build_vreg_node(value, arg));
        }
        return arg.map[value];
    }

    inline auto get_last_user(DAGValue *value, value_map_t &map) {
        if (value->users.empty()) return map[nullptr];
        else return value->users.back()->value();
    }

    template<typename Node, typename Inst>
    auto build_binary_node(Inst *inst, CallArg &arg) {
        auto node = std::make_unique<Node>(inst->type);
        node->lhs.set_link(get_node_value(inst->getLhs(), arg));
        node->rhs.set_link(get_node_value(inst->getRhs(), arg));
        arg.map[inst] = &node->ret;
        arg.dag.push_back(std::move(node));
    }

    template<typename Node, typename Inst>
    auto build_unary_node(Inst *inst, CallArg &arg) {
        auto inVal = ((mir::User *) inst)->getOperand(0);
        auto node = std::make_unique<Node>(inVal->type, inst->type);
        node->val.set_link(get_node_value(inVal, arg));
        arg.map[inst] = &node->ret;
        arg.dag.push_back(std::move(node));
    }

    inline auto build_load_node(mir::Instruction::load *load, CallArg &arg) {
        TODO("build_load_node");
    }

    inline auto build_store_node(mir::Instruction::store *store, CallArg &arg) {
        TODO("build_store_node");
    }

    inline auto build_getelementptr_node(mir::Instruction::getelementptr *gep, CallArg &arg) {
        TODO("build_getelementptr_node");
    }

    inline auto build_phi_node(mir::Instruction::phi *phi, CallArg &arg) {
        TODO("build_phi_node");
    }

    inline auto build_select_node(mir::Instruction::select *select, CallArg &arg) {
        TODO("build_select_node");
    }

#define build_inline(TY, func, ty) \
    case Instruction::TY: \
        func<node::ty>((Instruction::ty *) inst, arg); \
        break
#define build_exact(TY, ty) \
    case Instruction::TY: \
        build_##ty##_node((Instruction::ty *) inst, arg); \
        break
#define build_external(TY, ty) \
    case Instruction::TY: \
        std::invoke(callback, (Instruction::ty *) inst, arg); \
        break
    template<typename Callback>
    DAG build_dag_bb(mir::BasicBlock *bb, Callback &&callback) {
        using mir::Instruction;
        CallArg arg;
        auto entry = std::make_unique<node::EntryToken>();
        arg.map[nullptr] = &entry->ch;
        arg.last_side_effect = &entry->ch;
        arg.dag.push_back(std::move(entry));
        for (auto inst: bb->instructions) {
            switch (inst->instrTy) {
                build_external(RET, ret);
                build_external(BR, br);
                build_inline(ADD, build_binary_node, add);
                build_inline(SUB, build_binary_node, sub);
                build_inline(MUL, build_binary_node, mul);
                build_inline(UDIV, build_binary_node, udiv);
                build_inline(SDIV, build_binary_node, sdiv);
                build_inline(UREM, build_binary_node, urem);
                build_inline(SREM, build_binary_node, srem);
                build_inline(FADD, build_binary_node, fadd);
                build_inline(FSUB, build_binary_node, fsub);
                build_inline(FMUL, build_binary_node, fmul);
                build_inline(FDIV, build_binary_node, fdiv);
                build_inline(FREM, build_binary_node, frem);
                build_inline(SHL, build_binary_node, shl);
                build_inline(LSHR, build_binary_node, lshr);
                build_inline(ASHR, build_binary_node, ashr);
                build_inline(AND, build_binary_node, and_);
                build_inline(OR, build_binary_node, or_);
                build_inline(XOR, build_binary_node, xor_);
                build_inline(FNEG, build_unary_node, fneg);
                build_external(ALLOCA, alloca_);
                build_exact(LOAD, load);
                build_exact(STORE, store);
                build_exact(GETELEMENTPTR, getelementptr);
                build_inline(TRUNC, build_unary_node, trunc);
                build_inline(ZEXT, build_unary_node, zext);
                build_inline(SEXT, build_unary_node, sext);
                build_inline(FPTOUI, build_unary_node, fptoui);
                build_inline(FPTOSI, build_unary_node, fptosi);
                build_inline(UITOFP, build_unary_node, uitofp);
                build_inline(SITOFP, build_unary_node, sitofp);
                build_external(ICMP, icmp);
                build_external(FCMP, fcmp);
                build_exact(PHI, phi);
                build_exact(SELECT, select);
                build_external(CALL, call);
                build_external(MEMSET, memset);
            }
#ifdef _DEBUG_
            if (auto v = arg.map[inst])
                v->node->set_original(inst);
#endif
        }
        return std::move(arg.dag);
    }
#undef build_inline
#undef build_exact
#undef build_external

    template<typename Callback>
    std::vector<DAG> build_dag(mir::Function *func, Callback &&callback) {
        std::vector<DAG> dags;
        for (auto bb: func->bbs)
            dags.push_back(build_dag_bb(bb, callback));
        return dags;
    }
}

#endif //COMPILER_LIR_BUILD_H
