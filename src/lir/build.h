//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_LIR_BUILD_H
#define COMPILER_LIR_BUILD_H

#include <unordered_map>
#include "dag.h"

namespace LIR {
    template<typename K, typename V>
    using std_map = std::unordered_map<K, V>;
    using value_map_t = std_map<mir::Value *, DAGValue *>;

    struct FuncArg {
        ssize_t frame_cur = 0;
        size_t frame_size = 0;
        std_map<mir::Value *, ssize_t> address_map;

        void frame_shrink() {
            frame_size = std::max(frame_size, (size_t) -frame_cur);
        }
    };

    struct CallArg {
        DAG dag;
        value_map_t map;
        FuncArg *globals = nullptr;
        DAGValue *last_side_effect = nullptr;
    };

    inline auto build_constant_node(mir::Literal *literal, CallArg &arg) {
        auto node = std::make_unique<node::Constant>(literal);
        auto val = &node->value;
        arg.dag.push_back(std::move(node));
        return val;
    }

    inline std::unique_ptr<DAGNode> build_vreg_node(mir::Value *value, CallArg &arg) {
        if (auto it = arg.globals->address_map.find(value); it != arg.globals->address_map.end()) {
            auto node = std::make_unique<node::LAddress>(it->second);
            arg.map[value] = &node->value;
            return node;
        }
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
            return build_constant_node(lit, arg);
        } else if (auto gvar = dynamic_cast<mir::GlobalVar *>(value)) {
            arg.dag.push_back(build_gvar_node(gvar, arg.map));
        } else {
            arg.dag.push_back(build_vreg_node(value, arg));
        }
        return arg.map[value];
    }

    template<typename Node>
    auto build_convert_node(DAGValue *ori, mir::pType type, CallArg &arg) {
        if (ori->type == type) return ori;
        auto node = std::make_unique<Node>(ori->type, type);
        node->val.set_link(ori);
        auto val = &node->ret;
        arg.dag.push_back(std::move(node));
        return val;
    }

    inline auto get_last_user(DAGValue *value, value_map_t &map) {
        if (value->users.empty()) return map[nullptr];
        else return value->users.back()->value();
    }

    template<typename Node, typename Inst>
    void build_binary_node(Inst *inst, CallArg &arg) {
        auto node = std::make_unique<Node>(inst->type);
        node->lhs.set_link(get_node_value(inst->getLhs(), arg));
        node->rhs.set_link(get_node_value(inst->getRhs(), arg));
        arg.map[inst] = &node->ret;
        arg.dag.push_back(std::move(node));
    }

    template<typename Node, typename Inst>
    void build_unary_node(Inst *inst, CallArg &arg) {
        auto inVal = ((mir::User *) inst)->getOperand(0);
        auto node = std::make_unique<Node>(inVal->type, inst->type);
        node->val.set_link(get_node_value(inVal, arg));
        arg.map[inst] = &node->ret;
        arg.dag.push_back(std::move(node));
    }

    inline void build_alloca__node(mir::Instruction::alloca_ *alloca_, CallArg &arg) {
        arg.globals->frame_cur -= alloca_->type->ssize();
        arg.globals->frame_shrink();
        arg.globals->address_map[alloca_] = arg.globals->frame_cur;
    }

    inline void build_load_node(mir::Instruction::load *load, CallArg &arg) {
        auto addr = get_node_value(load->getPointerOperand(), arg);
        auto node = std::make_unique<node::LoadNode>(load->type);
        node->dependency.set_link(arg.last_side_effect);
        node->address.set_link(build_convert_node<node::zext>(addr, DAG_ADDRESS_TYPE, arg));
        arg.last_side_effect = &node->ch;
        arg.map[load] = &node->value;
        arg.dag.push_back(std::move(node));
    }

    inline void build_store_node(mir::Instruction::store *store, CallArg &arg) {
        auto addr = get_node_value(store->getDest(), arg);
        auto value = get_node_value(store->getSrc(), arg);
        auto node = std::make_unique<node::StoreNode>(value->type);
        node->dependency.set_link(arg.last_side_effect);
        node->address.set_link(build_convert_node<node::zext>(addr, DAG_ADDRESS_TYPE, arg));
        node->value.set_link(value);
        arg.last_side_effect = &node->ch;
        arg.dag.push_back(std::move(node));
    }

    inline void build_getelementptr_node(mir::Instruction::getelementptr *gep, CallArg &arg) {
        auto addr = get_node_value(gep->getPointerOperand(), arg);
        auto type = gep->indexTy;
        for (auto i = 0; i < gep->getNumIndices(); ++i) {
            if (i != 0) type = type->getBase();
            auto idx = gep->getIndexOperand(i);
            if (idx == mir::getIntegerLiteral(0)) continue;
            auto mul = std::make_unique<node::mul>(idx->type, mir::Type::getI32Type(), DAG_ADDRESS_TYPE);
            mul->lhs.set_link(get_node_value(idx, arg));
            mul->rhs.set_link(get_node_value(mir::getLiteral((int)type->ssize()), arg));
            auto add = std::make_unique<node::add>(addr->type, DAG_ADDRESS_TYPE, DAG_ADDRESS_TYPE);
            add->lhs.set_link(addr);
            add->rhs.set_link(&mul->ret);
            addr = &add->ret;
            arg.dag.push_back(std::move(mul));
            arg.dag.push_back(std::move(add));
        }
        arg.map[gep] = addr;
    }

    inline void build_phi_node(mir::Instruction::phi *phi, CallArg &arg) {
        TODO("build_phi_node");
    }

    inline void build_select_node(mir::Instruction::select *select, CallArg &arg) {
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
    DAG build_dag_bb(mir::BasicBlock *bb, FuncArg &func_arg, Callback &&callback) {
        using mir::Instruction;
        CallArg arg;
        arg.globals = &func_arg;
        auto entry = std::make_unique<node::EntryToken>();
#ifdef _DEBUG_
        entry->set_original(bb->name);
#endif
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
                build_exact(ALLOCA, alloca_);
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
            if (auto it = arg.map.find(inst); it != arg.map.end())
                it->second->node->set_original(inst->name);
#endif
        }
        return std::move(arg.dag);
    }

#undef build_inline
#undef build_exact
#undef build_external

    template<typename Callback>
    std::vector<DAG> build_dag(mir::Function *func, Callback &&callback) {
        FuncArg arg;
        std::vector<DAG> dags;
        for (auto bb: func->bbs)
            dags.push_back(build_dag_bb(bb, arg, callback));
        return dags;
    }
}

#endif //COMPILER_LIR_BUILD_H
