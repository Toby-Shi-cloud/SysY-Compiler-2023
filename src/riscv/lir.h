//
// Created by toby on 2024/6/22.
//

#ifndef COMPILER_RISCV_LIR_H
#define COMPILER_RISCV_LIR_H

#define ARCHITECTURE_XLEN 64
#include "../lir/dag.h"
#include "reg.h"

namespace riscv::node {
using namespace lir64;
using namespace lir64::node;

struct RiscvNode : DAGNode {
    [[nodiscard]] bool in_used() const override { return true; }
};

struct RegisterNode : RiscvNode {
    Register reg;
    DAGValue value;
    explicit RegisterNode(Register reg) : reg(reg), value(this, reg.type()) {}
    [[nodiscard]] const_slice<DAGLink> links() const override { return {nullptr, 0}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {&value, 1}; }
    [[nodiscard]] std::string name() const override { return std::string("Register ") + reg.name(); }
};

struct RetNode : RiscvNode {
    DAGLink ret{this, mir::Type::getI64Type()}, ch{this, DAGValue::Ch()}, glue{this, DAGValue::Glue()};
    [[nodiscard]] const_slice<DAGLink> links() const override { return {&ret, 3}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {nullptr, 0}; }
    [[nodiscard]] std::string name() const override { return "Ret"; }
};

struct RetVoidNode : RiscvNode {
    DAGLink ch{this, DAGValue::Ch()};
    [[nodiscard]] const_slice<DAGLink> links() const override { return {&ch, 1}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {nullptr, 0}; }
    [[nodiscard]] std::string name() const override { return "Ret"; }
};

struct CallNode : RiscvNode {
    std::string func;
    std::vector<DAGLink> deps;  // arg (at most 8) + ch
    DAGValue ch{this, DAGValue::Ch()}, glue{this, DAGValue::Glue()};
    CallNode(std::string func, size_t args) : func(std::move(func)) {
        deps.resize(args + 1, DAGLink{this, DAGValue::Ch()});
    }
    [[nodiscard]] const_slice<DAGLink> links() const override { return {deps.data(), deps.size()}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {&ch, 2}; }
    [[nodiscard]] std::string name() const override { return "Call " + func; }
    [[nodiscard]] DAGValue *ch_value() override { return &ch; };
};

struct MemsetNode : RiscvNode {
    mir::Instruction::memset *inst;
    DAGLink dependency{this, DAGValue::Ch()}, address{this, DAG_ADDRESS_TYPE};
    DAGValue ch{this, DAGValue::Ch()};
    explicit MemsetNode(mir::Instruction::memset *inst) : inst(inst) {}
    [[nodiscard]] const_slice<DAGLink> links() const override { return {&dependency, 2}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {&ch, 1}; }
    [[nodiscard]] std::string name() const override {
        return "memset\\<" + std::to_string(inst->val) + "\\>(" + std::to_string(inst->size) + ")";
    }
    [[nodiscard]] DAGValue *ch_value() override { return &ch; };
};

struct Riscv1I1ONode : RiscvNode {
    DAGLink rs;
    DAGValue rd;
    Riscv1I1ONode(DAGValue::Type rs, DAGValue::Type rd) : rs(this, rs), rd(this, rd) {}
    [[nodiscard]] const_slice<DAGLink> links() const override { return {&rs, 1}; }
    [[nodiscard]] const_slice<DAGValue> values() const override { return {&rd, 1}; }
};

#define Riscv1I1O(NODE, INST, rs, rd)                                                     \
    struct NODE : Riscv1I1ONode {                                                         \
        NODE() : Riscv1I1ONode(mir::Type::get##rs##Type(), mir::Type::get##rd##Type()) {} \
        [[nodiscard]] std::string name() const override { return INST; }                  \
    };

Riscv1I1O(FMV_X_W, "fmv.x.w", Float, I64)      // fmv.x.w rd(x?), rs(f?)
    Riscv1I1O(FMV_W_X, "fmv.w.x", I64, Float)  // fmv.w.x rd(f?), rs(x?)

#undef Riscv1I1O
}  // namespace riscv::node

namespace riscv {
std::vector<lir64::DAG> build_dag(mir::Function *func);
}

#endif  // COMPILER_RISCV_LIR_H
