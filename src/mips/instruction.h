//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_INSTRUCTION_H
#define COMPILER_MIPS_INSTRUCTION_H

#include "backend/component.h"
#include "mips/operand.h"

namespace backend::mips {
struct Instruction : InstructionBase {
    // clang-format off
    enum class Ty {
        NOP, ADDU, SUBU, AND, OR, XOR, SLLV, SRAV, SRLV, SLT, SLTU, MOVN, MOVZ, CLO, CLZ,
        MUL, MULTU, MADDU, MSUBU, DIV, DIVU,
        MOVE, MFHI, MFLO, MTHI, MTLO,
        ADDIU, ANDI, ORI, XORI, SLL, SRL, SRA, SLTI, SLTIU, LUI, LI,
        LA, LW, LWL, LWR, LB, LH, LHU, LBU, SW, SWL, SWR, SB, SH,
        BEQ, BNE, BGEZ, BGTZ, BLEZ, BLTZ, BGEZAL, BLTZAL,
        J, JR, JAL, JALR, SYSCALL
    } ty;
    // clang-format on

    explicit Instruction(Ty ty) noexcept : Instruction(ty, {}, {}) {}

    Instruction(Ty ty, std::vector<rRegister> regDef, std::vector<rRegister> regUse) noexcept
        : InstructionBase(std::move(regDef), std::move(regUse)), ty{ty} {
        if (isFuncCallImpl()) {
            for (auto reg : PhyRegister::get(&PhyRegister::isArg)) reg_use_push_back(reg);
            for (auto reg : PhyRegister::get(&PhyRegister::isRet)) reg_def_push_back(reg);
        }
    }

    Instruction(const Instruction &inst) noexcept = default;
    Instruction(Instruction &&inst) noexcept = delete;

    [[nodiscard]] bool isFuncCall() const override { return isFuncCallImpl(); }
    [[nodiscard]] bool isJumpBranch() const override { return ty >= Ty::BEQ && ty <= Ty::JALR; }
    [[nodiscard]] bool isConditionalBranch() const { return ty >= Ty::BEQ && ty <= Ty::BLTZ; }
    [[nodiscard]] bool isUnconditionalJump() const { return ty >= Ty::J && ty <= Ty::JR; }
    [[nodiscard]] bool isSyscall() const { return ty == Ty::SYSCALL; }
    [[nodiscard]] bool isStore() const { return ty >= Ty::SW && ty <= Ty::SH; }
    [[nodiscard]] bool isLoad() const { return ty >= Ty::LA && ty <= Ty::LBU; }
    [[nodiscard]] rLabel getJumpLabel() const override { return nullptr; }
    void setJumpLabel(rLabel newLabel) override {}
    std::ostream &output(std::ostream &os) const override { return os << ty; }

 private:
    [[nodiscard]] bool isFuncCallImpl() const {
        return ty == Ty::JAL || ty == Ty::JALR || ty == Ty::BGEZAL || ty == Ty::BLTZAL;
    }
};

template <typename T>
struct InstructionImpl : Instruction {
    using Instruction::Instruction;

    [[nodiscard]] pInstructionBase clone() const override {
        return std::make_unique<T>(static_cast<const T &>(*this));
    }
};

struct BinaryRInst : InstructionImpl<BinaryRInst> {
    explicit BinaryRInst(Ty ty, rRegister dst, rRegister src1, rRegister src2)
        : InstructionImpl{ty, {dst}, {src1, src2}} {
        if (ty == Ty::MOVN || ty == Ty::MOVZ) reg_use_push_back(dst);
        assert(ty >= Ty::ADDU && ty <= Ty::MOVZ);
    }

    explicit BinaryRInst(Ty ty, rRegister dst, rRegister src) : InstructionImpl{ty, {dst}, {src}} {
        assert(ty == Ty::CLO || ty == Ty::CLZ);
    }

    [[nodiscard]] rRegister dst() const { return regDef[0]; }
    [[nodiscard]] rRegister src1() const { return regUse[0]; }
    [[nodiscard]] rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t" << dst() << ", " << src1();
        if (src2()) os << ", " << src2();
        return os;
    }
};

struct BinaryIInst : InstructionImpl<BinaryIInst> {
    pImmediate imm;

    BinaryIInst(const BinaryIInst &inst)
        : InstructionImpl(inst), imm(new Immediate(inst.imm->value)) {}
    explicit BinaryIInst(Ty ty, rRegister dst, rRegister src, int imm)
        : InstructionImpl{ty, {dst}, {src}}, imm(new Immediate(imm)) {}
    explicit BinaryIInst(Ty ty, rRegister dst, rRegister src, pImmediate imm)
        : InstructionImpl{ty, {dst}, {src}}, imm(std::move(imm)) {}
    explicit BinaryIInst(Ty ty, rRegister dst, int imm)
        : InstructionImpl{ty, {dst}, {}}, imm(new Immediate(imm)) {}
    explicit BinaryIInst(Ty ty, rRegister dst, pImmediate imm)
        : InstructionImpl{ty, {dst}, {}}, imm(std::move(imm)) {}

    [[nodiscard]] rRegister dst() const { return regDef[0]; }
    [[nodiscard]] rRegister src() const { return regUse.empty() ? nullptr : regUse[0]; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t" << dst();
        if (src()) os << ", " << src();
        os << ", " << imm;
        return os;
    }
};

struct BinaryMInst : InstructionImpl<BinaryMInst> {
    explicit BinaryMInst(Ty ty, rRegister src1, rRegister src2)
        : InstructionImpl{ty, {PhyRegister::HI, PhyRegister::LO}, {src1, src2}} {
        assert(ty != Ty::MUL);
        if (ty == Ty::MADDU || ty == Ty::MSUBU)
            reg_use_push_back(PhyRegister::HI), reg_use_push_back(PhyRegister::LO);
    }

    explicit BinaryMInst(Ty ty, rRegister dst, rRegister src1, rRegister src2)
        : InstructionImpl{ty, {dst, PhyRegister::HI, PhyRegister::LO}, {src1, src2}} {
        assert(ty == Ty::MUL);
    }

    [[nodiscard]] rRegister dst() const { return ty == Ty::MUL ? regDef[0] : nullptr; }
    [[nodiscard]] rRegister src1() const { return regUse[0]; }
    [[nodiscard]] rRegister src2() const { return regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t";
        if (dst()) os << dst() << ", ";
        os << src1() << ", " << src2();
        return os;
    }
};

struct LoadInst : InstructionImpl<LoadInst> {
    rLabel label;
    pImmediate offset;

    LoadInst(const LoadInst &inst)
        : InstructionImpl(inst), label(inst.label), offset(new Immediate(inst.offset->value)) {}
    explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset)
        : InstructionImpl{ty, {dst}, {base}}, label(nullptr), offset(new Immediate(offset)) {}
    explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset, const int *immBase)
        : InstructionImpl{ty, {dst}, {base}},
          label(nullptr),
          offset(new DynImmediate(offset, immBase)) {}
    explicit LoadInst(Ty ty, rRegister dst, rLabel label)
        : InstructionImpl{ty, {dst}, {}}, label(label), offset(nullptr) {}
    explicit LoadInst(Ty ty, rRegister dst, rAddress address)
        : InstructionImpl{ty, {dst}, {address->base}},
          label(address->label),
          offset(address->offset->clone()) {}

    [[nodiscard]] rRegister dst() const { return regDef[0]; }
    [[nodiscard]] rRegister base() const { return regUse[0]; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t" << dst() << ", ";
        if (label) os << label;
        if (label && offset) os << " + ";
        if (offset) os << offset << "(" << base() << ")";
        return os;
    }
};

struct StoreInst : InstructionImpl<StoreInst> {
    rLabel label;
    pImmediate offset;

    StoreInst(const StoreInst &inst)
        : InstructionImpl(inst), label(inst.label), offset(new Immediate(inst.offset->value)) {}
    explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset)
        : InstructionImpl{ty, {}, {src, base}}, label(nullptr), offset(new Immediate(offset)) {}
    explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset, const int *immBase)
        : InstructionImpl{ty, {}, {src, base}},
          label(nullptr),
          offset(new DynImmediate(offset, immBase)) {}
    explicit StoreInst(Ty ty, rRegister src, rLabel label)
        : InstructionImpl{ty, {}, {src}}, label(label), offset(nullptr) {}
    explicit StoreInst(Ty ty, rRegister src, rAddress address)
        : InstructionImpl{ty, {}, {src, address->base}},
          label(address->label),
          offset(address->offset->clone()) {}

    [[nodiscard]] rRegister src() const { return regUse[0]; }
    [[nodiscard]] rRegister base() const { return regUse[1]; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t" << src() << ", ";
        if (label) os << label;
        if (label && offset) os << " + ";
        if (offset) os << offset << "(" << base() << ")";
        return os;
    }
};

struct BranchInst : InstructionImpl<BranchInst> {
    rLabel label;

    explicit BranchInst(Ty ty, rRegister src1, rRegister src2, rLabel label)
        : InstructionImpl{ty, {}, {src1, src2}}, label(label) {}
    explicit BranchInst(Ty ty, rRegister src1, rLabel label)
        : InstructionImpl{ty, {}, {src1}}, label(label) {}

    [[nodiscard]] rRegister src1() const { return regUse.empty() ? nullptr : regUse[0]; }
    [[nodiscard]] rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }
    [[nodiscard]] rLabel getJumpLabel() const override { return label; }
    void setJumpLabel(rLabel newLabel) override { this->label = newLabel; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t" << src1() << ", ";
        if (src2()) os << src2() << ", ";
        os << label;
        return os;
    }
};

struct MoveInst : InstructionImpl<MoveInst> {
    explicit MoveInst(rRegister dst, rRegister src)
        : InstructionImpl{deduce_type(dst, src), {dst}, {src}} {}

    [[nodiscard]] rRegister dst() const { return regDef[0]; }
    [[nodiscard]] rRegister src() const { return regUse[0]; }

    std::ostream &output(std::ostream &os) const override {
        if (ty == Ty::MOVE) return os << ty << "\t" << dst() << ", " << src();
        if (ty == Ty::MFHI || ty == Ty::MFLO) return os << ty << "\t" << dst();
        return os << ty << "\t" << src();
    }

 private:
    static Ty deduce_type(rRegister dst, rRegister src) {
        auto dst_is_hi_lo = dst == PhyRegister::HI || dst == PhyRegister::LO;
        auto src_is_hi_lo = src == PhyRegister::HI || src == PhyRegister::LO;
        assert(!dst_is_hi_lo || !src_is_hi_lo);
        if (!dst_is_hi_lo && !src_is_hi_lo) return Ty::MOVE;
        if (dst_is_hi_lo) return dst == PhyRegister::HI ? Ty::MTHI : Ty::MTLO;
        return src == PhyRegister::HI ? Ty::MFHI : Ty::MFLO;
    }
};

struct JumpInst : InstructionImpl<JumpInst> {
    rLabel label;

    explicit JumpInst(Ty ty, rLabel label) : InstructionImpl{ty}, label(label) {}
    explicit JumpInst(Ty ty, rRegister tar) : InstructionImpl{ty, {}, {tar}}, label(nullptr) {}
    explicit JumpInst(Ty ty, rRegister ra, rRegister tar)
        : InstructionImpl{ty, {}, {ra, tar}}, label(nullptr) {}

    [[nodiscard]] rRegister target() const { return regUse.empty() ? nullptr : regUse.back(); }
    [[nodiscard]] rRegister ra() const { return regUse.size() <= 1 ? nullptr : regUse.front(); }
    [[nodiscard]] rLabel getJumpLabel() const override { return label; }
    void setJumpLabel(rLabel newLabel) override { this->label = newLabel; }

    std::ostream &output(std::ostream &os) const override {
        os << ty << "\t";
        if (label)
            os << label;
        else if (ra())
            os << ra() << ", " << target();
        else
            os << target();
        return os;
    }
};

struct SyscallInst : InstructionImpl<SyscallInst> {
    enum class SyscallId : unsigned {
        PrintInteger = 1,
        PrintString = 4,
        ReadInteger = 5,
        ExitProc = 10,
        PrintCharacter = 11,
    };

    static std::pair<pBinaryIInst, pSyscallInst> syscall(SyscallId id) {
        pBinaryIInst bin{new BinaryIInst(Ty::LI, PhyRegister::get("$v0"), static_cast<int>(id))};
        pSyscallInst sys{new SyscallInst()};
        sys->reg_use_push_back(PhyRegister::get("$v0"));
        switch (id) {
        case SyscallId::PrintInteger:
        case SyscallId::PrintString:
        case SyscallId::PrintCharacter: sys->reg_use_push_back(PhyRegister::get("$a0")); break;
        case SyscallId::ReadInteger: sys->reg_def_push_back(PhyRegister::get("$v0")); break;
        case SyscallId::ExitProc: break;
        }
        return {std::move(bin), std::move(sys)};
    }

 private:
    explicit SyscallInst() : InstructionImpl{Ty::SYSCALL} {}
};

inline std::ostream &operator<<(std::ostream &os, const Instruction &inst) {
    return inst.output(os);
}
}  // namespace backend::mips

#ifdef DBG_ENABLE
namespace dbg {
inline std::string instruction_to_string(const backend::mips::Instruction &value) {
    std::stringstream ss;
    ss << value;
    auto s = ss.str();
    for (auto &ch : s)
        if (ch == '\t') ch = ' ';
    return '"' + s + '"';
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::mips::rInstruction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << instruction_to_string(*value);
    return true;
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::mips::pInstruction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << instruction_to_string(*value);
    return true;
}
}  // namespace dbg
#endif  // DBG_ENABLE

#endif  // COMPILER_MIPS_INSTRUCTION_H
