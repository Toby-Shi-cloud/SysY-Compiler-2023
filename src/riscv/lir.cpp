//
// Created by toby on 2024/6/21.
//

#include "lir.h"
#include "../lir/build.h"

using mir::Instruction, lir64::CallArg, mir::Type;
using namespace riscv;

static auto build_to_reg_node(mir::Value *value, node::DAGValue *reg, CallArg &arg) {
    auto copy = std::make_unique<node::CopyToReg>(Type::getI64Type());
    copy->dependency.set_link(arg.last_side_effect);
    copy->reg.set_link(reg);
    auto val = lir64::get_node_value(value, arg);
    if (value->type == Type::getI32Type() || value->type->isPointerTy() || value->type->isArrayTy()) {
        auto sext = lir64::build_convert_node<node::sext>(val, Type::getI64Type(), arg);
        copy->value.set_link(sext);
    } else if (value->type == Type::getFloatType()) {
        auto move = std::make_unique<node::FMV_X_W>();
        move->rs.set_link(val);
        copy->value.set_link(&move->rd);
        arg.dag.push_back(std::move(move));
    } else {
        TODO("unknown value type");
    }
    arg.last_side_effect = &copy->ch;
    return copy;
}

static auto build_from_reg_node(mir::Value *value, node::DAGValue *reg, CallArg &arg) {
    auto copy = std::make_unique<node::CopyFromReg>(reg->type);
    copy->dependency.set_link(arg.last_side_effect);
    copy->reg.set_link(reg);
    if (value->type == Type::getI32Type()) {
        arg.map[value] = lir64::build_convert_node<node::trunc>(&copy->value, Type::getI32Type(), arg);
    } else if (value->type == Type::getFloatType()) {
        auto move = std::make_unique<node::FMV_W_X>();
        move->rs.set_link(&copy->value);
        arg.map[value] = &move->rd;
        arg.dag.push_back(std::move(move));
    } else {
        TODO("unknown value type");
    }
    arg.last_side_effect = &copy->ch;
    return copy;
}

std::vector<lir64::DAG> riscv::build_dag(mir::Function *func) {
    return lir64::build_dag(func, overloaded{
        [](Instruction::ret *ret, CallArg &arg) {
            if (ret->getReturnValue() == nullptr) {
                auto node = std::make_unique<node::RetVoidNode>();
                node->ch.set_link(arg.last_side_effect);
                arg.dag.push_back(std::move(node));
            } else {
                auto reg = std::make_unique<node::RegisterNode>("a0"_R);
                auto copy = build_to_reg_node(ret->getReturnValue(), &reg->value, arg);
                auto node = std::make_unique<node::RetNode>();
                node->ret.set_link(&reg->value);
                node->ch.set_link(arg.last_side_effect);
                node->glue.set_link(&copy->glue);
                arg.dag.push_back(std::move(reg));
                arg.dag.push_back(std::move(copy));
                arg.dag.push_back(std::move(node));
            }
        },
        [](Instruction::call *call, CallArg &arg) {
            if (call->getNumArgs() > 8) TODO("call with more than 8 args");
            auto node = std::make_unique<node::CallNode>(call->getFunction()->name.substr(1), call->getNumArgs());
            auto last_side_effect = arg.last_side_effect;
            for (auto i = 0; i < call->getNumArgs(); ++i) {
                arg.last_side_effect = last_side_effect;
                auto reg = std::make_unique<node::RegisterNode>(Register{false, ("a0"_R).id + i});
                auto copy = build_to_reg_node(call->getArg(i), &reg->value, arg);
                node->deps[i].set_link(&copy->ch);
                arg.dag.push_back(std::move(reg));
                arg.dag.push_back(std::move(copy));
            }
            node->deps.back().set_link(last_side_effect);
            arg.last_side_effect = &node->ch;
            if (call->type->isVoidTy()) {
                arg.dag.push_back(std::move(node));
                return;
            }
            auto reg = std::make_unique<node::RegisterNode>("a0"_R);
            auto copy = build_from_reg_node(call, &reg->value, arg);
            copy->glue.set_link(&node->glue);
            arg.dag.push_back(std::move(reg));
            arg.dag.push_back(std::move(copy));
            arg.dag.push_back(std::move(node));
        },
        [](Instruction::memset *memset, CallArg &arg) {
            auto node = std::make_unique<node::MemsetNode>(memset);
            node->dependency.set_link(arg.last_side_effect);
            auto addr = lir64::get_node_value(memset->getBase(), arg);
            node->address.set_link(lir64::build_convert_node<node::zext>(addr, DAG_ADDRESS_TYPE, arg));
            arg.last_side_effect = &node->ch;
            arg.dag.push_back(std::move(node));
        },
    });
}
