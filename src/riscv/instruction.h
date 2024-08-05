//
// Created by toby on 2024/8/4.
//

#ifndef COMPILER_RISCV_INSTRUCTION_H
#define COMPILER_RISCV_INSTRUCTION_H

#include "backend/operand.h"
#include "dbg.h"

#include "backend/component.h"  // IWYU pragma: export
#include "riscv/operand.h"      // IWYU pragma: export

namespace backend::riscv {
struct Instruction : InstructionBase {
    // clang-format off
    enum class Ty {
        NOP,
        // basic (RV32I)
        LUI, AUIPC, JAL, JALR,
        BEQ, BNE, BLT, BGE, BLTU, BGEU,
        LB, LH, LW, LBU, LHU, SB, SH, SW,
        ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
        ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
        // basic (RV64I)
        LWU, LD, SD, ADDIW, SLLIW, SRLIW, SRAIW,
        ADDW, SUBW, SLLW, SRLW, SRAW,
        // multiply and divide (RV32M)
        MUL, MULH,  MULHSU, MULHU, DIV, DIVU, REM, REMU,
        // multiply and divide (RV64M)
        MULW, DIVW, DIVUW, REMW, REMUW,
        // float (RV32F)
        FLW, FSW, FMADD_S, MFSUB_S, FNMADD_S, FNMSUB_S, FADD_S, FSUB_S, FMUL_S,
        FDIV_S, FSQRT_S, FSGNJ_S, FSGNJN_S, FSGNJX_S, FMIN_S, FMAX_S,
        FCVT_W_S, FCVT_WU_S, FMV_X_W, FEQ_S, FLT_S, FLE_S,
        FCLASS_S, FCVT_S_W, FCVT_S_WU, FMV_W_X,
        // float (RV64F)
        FCVT_L_S, FCVT_LU_S, FCVT_S_L, FCVT_S_LU,
        // useful pseudo instructions
        CALL, RET, J, MV,
    } ty;
    // clang-format on
    friend constexpr bool floatOp(Ty ty) { return ty >= Ty::FLW && ty <= Ty::FCVT_S_LU; }

    enum class InstType { R, I, S, B, U, J, Pseudo };
    virtual InstType getInstType() const = 0;

    explicit Instruction(Ty ty) noexcept : Instruction(ty, {}, {}) {}

    Instruction(Ty ty, std::vector<rRegister> regDef, std::vector<rRegister> regUse) noexcept
        : InstructionBase(std::move(regDef), std::move(regUse)), ty{ty} {
        if (isFuncCallImpl()) {
            for (auto reg : PhyRegister::gets(&PhyRegister::isArg)) reg_use_push_back(reg);
            for (auto reg : PhyRegister::gets(&PhyRegister::isRet)) reg_def_push_back(reg);
        }
    }

    Instruction(const Instruction &inst) noexcept = default;
    Instruction(Instruction &&inst) noexcept = delete;

    [[nodiscard]] bool isFuncCall() const override { return isFuncCallImpl(); }
    [[nodiscard]] bool isJumpBranch() const override {
        return ty >= Ty::JAL && ty <= Ty::BGEU || ty >= Ty::CALL && ty <= Ty::J;
    }

    [[nodiscard]] bool isConditionalBranch() const { return ty >= Ty::BEQ && ty <= Ty::BGEU; }
    [[nodiscard]] bool isUnconditionalJump() const {
        return ty == Ty::CALL || ty == Ty::RET || ty == Ty::JAL || ty == Ty::JALR || ty == Ty::J;
    }

    [[nodiscard]] bool isStore() const {
        return ty >= Ty::SB && ty <= Ty::SW || ty == Ty::SD || ty == Ty::FSW;
    }
    [[nodiscard]] bool isLoad() const {
        return ty >= Ty::LB && ty <= Ty::LHU || ty == Ty::LWU || ty == Ty::LD | ty == Ty::FLW;
    }

    [[nodiscard]] rLabel getJumpLabel() const override { return nullptr; }
    void setJumpLabel(rLabel newLabel) override {}

 private:
    [[nodiscard]] bool isFuncCallImpl() const { return ty == Ty::CALL || ty == Ty::JAL; }
};

inline std::ostream &operator<<(std::ostream &os, Instruction::Ty ty) {
    auto name = magic_enum::enum_name(ty);
    for (auto c : name) os << (char)(c == '_' ? '.' : ::tolower(c));
    return os;
}

inline std::ostream &operator<<(std::ostream &os, Instruction::InstType t) {
    return os << magic_enum::enum_name(t);
}

#define CLONE_DECL(self) CLONED(InstructionBase, self)

// rd = rs1 op rs2
// add, sub, etc
struct RInstruction : Instruction {
    CLONE_DECL(RInstruction);
    InstType getInstType() const override { return InstType::R; }

    RInstruction(Ty ty, rRegister rd, rRegister rs1, rRegister rs2)
        : Instruction(ty, {rd}, {rs1, rs2}) {}

    rRegister rd() const { return regDef[0]; }
    rRegister rs1() const { return regUse[0]; }
    rRegister rs2() const { return regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rd() << ", " << rs1() << ", " << rs2();
    }
};

// rd = rs1 op imm(12)
// load, addi, etc
struct IInstruction : Instruction {
    pImmediate imm;
    IInstruction(const IInstruction &inst) : Instruction{inst}, imm{inst.imm->clone()} {}

    CLONE_DECL(IInstruction);
    InstType getInstType() const override { return InstType::I; }

    IInstruction(Ty ty, rRegister rd, rRegister rs1, pImmediate imm)
        : Instruction(ty, {rd}, {rs1}), imm{std::move(imm)} {}

    rRegister rd() const { return regDef[0]; }
    rRegister rs1() const { return regUse[0]; }

    std::ostream &output(std::ostream &os) const override {
        return isLoad() ? os << ty << '\t' << rd() << ", " << imm << '(' << rs1() << ')'
                        : os << ty << '\t' << rd() << ", " << rs1() << ", " << imm;
    }
};

// rs1, rs2, imm(12)
// store
struct SInstruction : Instruction {
    pImmediate imm;
    SInstruction(const SInstruction &inst) : Instruction{inst}, imm{inst.imm->clone()} {}

    CLONE_DECL(SInstruction);
    InstType getInstType() const override { return InstType::S; }

    SInstruction(Ty ty, rRegister rs1, rRegister rs2, pImmediate imm)
        : Instruction(ty, {}, {rs1, rs2}), imm{std::move(imm)} {}

    rRegister rs1() const { return regUse[0]; }
    rRegister rs2() const { return regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rs1() << ", " << imm << "(" << rs2() << ")";
    }
};

// rs1, rs2, label
// branch
struct BInstruction : Instruction {
    rLabel label;

    CLONE_DECL(BInstruction);
    InstType getInstType() const override { return InstType::B; }
    rLabel getJumpLabel() const override { return label; }
    void setJumpLabel(rLabel newLabel) override { label = newLabel; }

    BInstruction(Ty ty, rRegister rs1, rRegister rs2, rLabel label)
        : Instruction(ty, {}, {rs1, rs2}), label{label} {}

    rRegister rs1() const { return regUse[0]; }
    rRegister rs2() const { return regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rs1() << ", " << rs2() << ", " << label;
    }
};

// rd = imm(31:12)
// lui
struct UInstruction : Instruction {
    pImmediate imm;
    UInstruction(const UInstruction &inst)
        : Instruction{inst}, imm{inst.imm->clone()} {}

    CLONE_DECL(UInstruction);
    InstType getInstType() const override { return InstType::U; }

    UInstruction(Ty ty, rRegister rd, pImmediate imm)
        : Instruction(ty, {rd}, {}), imm{std::move(imm)} {}

    rRegister rd() const { return regDef[0]; }

    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rd() << ", " << imm;
    }
};

// rd = $pc
// jal
struct JInstruction : Instruction {
    rLabel label;

    CLONE_DECL(JInstruction);
    InstType getInstType() const override { return InstType::J; }
    rLabel getJumpLabel() const override { return label; }
    void setJumpLabel(rLabel newLabel) override { label = newLabel; }

    JInstruction(Ty ty, rRegister rd, rLabel label)
        : Instruction(ty, {rd}, {}), label{label} {}

    rRegister rd() const { return regDef[0]; }

    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rd() << ", " << label;
    }
};

struct CallInstruction : Instruction {
    rLabel label;
    CLONE_DECL(CallInstruction);
    InstType getInstType() const override { return InstType::Pseudo; }
    explicit CallInstruction(rLabel label) : Instruction(Ty::CALL), label{label} {}
    rLabel getJumpLabel() const override { return label; }
    std::ostream &output(std::ostream &os) const override { return os << ty << '\t' << label; }
};

struct RetInstruction : Instruction {
    CLONE_DECL(RetInstruction);
    InstType getInstType() const override { return InstType::Pseudo; }
    RetInstruction() : Instruction(Ty::RET) {}
    std::ostream &output(std::ostream &os) const override { return os << ty; }
};

struct JumpInstruction : Instruction {
    rLabel label;
    CLONE_DECL(JumpInstruction);
    InstType getInstType() const override { return InstType::Pseudo; }
    explicit JumpInstruction(rLabel label) : Instruction(Ty::J), label{label} {}
    rLabel getJumpLabel() const override { return label; }
    void setJumpLabel(rLabel newLabel) override { label = newLabel; }
    std::ostream &output(std::ostream &os) const override { return os << ty << '\t' << label; }
};

struct MoveInstruction : Instruction {
    CLONE_DECL(MoveInstruction);
    InstType getInstType() const override { return InstType::Pseudo; }
    MoveInstruction(rRegister rd, rRegister rs) : Instruction(Ty::MV, {rd}, {rs}) {}
    rRegister rd() const { return regDef[0]; }
    rRegister rs() const { return regUse[0]; }
    std::ostream &output(std::ostream &os) const override {
        return os << ty << '\t' << rd() << ", " << rs();
    }
};

#undef CLONE_DECL

}  // namespace backend::riscv

#ifdef DBG_ENABLE
namespace dbg {
inline std::string instruction_to_string(const backend::riscv::Instruction &value) {
    std::stringstream ss;
    ss << value;
    auto s = ss.str();
    for (auto &ch : s)
        if (ch == '\t') ch = ' ';
    return '"' + s + '"';
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::riscv::rInstruction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << instruction_to_string(*value);
    return true;
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::riscv::pInstruction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << instruction_to_string(*value);
    return true;
}
}  // namespace dbg
#endif  // DBG_ENABLE

#endif  // COMPILER_RISCV_INSTRUCTION_H
