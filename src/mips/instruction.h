//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_INSTRUCTION_H
#define COMPILER_MIPS_INSTRUCTION_H

#include "operand.h"
#include "../enum.h"

namespace mips {
    struct Block;
}

namespace mips {
    struct Instruction {
        enum class Ty {
            NOP, ADDU, SUBU, AND, OR, NOR, XOR, SLLV, SRAV, SRLV, SLT, SLTU, MOVN, MOVZ, MUL,
            MULT, MULTU, MADD, MADDU, MSUB, MSUBU, DIV, DIVU, CLO, CLZ, MOVE,
            ADDIU, ANDI, ORI, XORI, SLL, SRL, SRA, SLTI, SLTIU, LUI, LI, REM, REMU,
            LA, LW, LWL, LWR, LB, LH, LHU, LBU, SW, SWL, SWR, SB, SH,
            BEQ, BNE, BGEZ, BGTZ, BLEZ, BLTZ, BGEZAL, BLTZAL,
            J, JR, JAL, JALR, MFHI, MFLO, MTHI, MTLO, SYSCALL
        } ty;
        std::vector<rRegister> regDef, regUse;
        inst_node_t node{}; // the position where the inst is. node for jump/branch inst is broken and DO NOT USE!
        rSubBlock parent{};

        virtual ~Instruction() {
            for (auto reg: regDef) reg->defUsers.erase(this);
            for (auto reg: regUse) reg->useUsers.erase(this);
        }

        explicit Instruction(Ty ty) : ty{ty} {}

        explicit Instruction(Ty ty, std::vector<rRegister> regDef, std::vector<rRegister> regUse)
                : ty{ty}, regDef{std::move(regDef)}, regUse{std::move(regUse)} {
            for (auto reg: this->regDef) reg->defUsers.insert(this);
            for (auto reg: this->regUse) reg->useUsers.insert(this);
        }

        Instruction(const Instruction &inst) noexcept: Instruction(inst.ty, inst.regDef, inst.regUse) {}

        Instruction(Instruction &&inst) noexcept = delete;

        inline void reg_def_push_back(rRegister reg) {
            regDef.push_back(reg);
            reg->defUsers.insert(this);
        }

        inline void reg_use_push_back(rRegister reg) {
            regUse.push_back(reg);
            reg->useUsers.insert(this);
        }

        template<bool visitDef = true, bool visitUse = true, typename T>
        inline void for_each_reg(T &&func) {
            if constexpr (visitDef) for (auto reg: regDef) func(reg);
            if constexpr (visitUse) for (auto reg: regUse) func(reg);
        }

        template<typename T>
        inline void for_each_use_reg(T &&func) { for_each_reg<false, true>(func); }

        template<typename T>
        inline void for_each_def_reg(T &&func) { for_each_reg<true, false>(func); }

        template<bool visitDef = true, bool visitUse = true, typename T>
        inline void for_each_vreg(T &&func) {
            if constexpr (visitDef)
                for (auto reg: regDef)
                    if (auto vir = dynamic_cast<mips::rVirRegister>(reg))
                        func(vir);
            if constexpr (visitUse)
                for (auto reg: regUse)
                    if (auto vir = dynamic_cast<mips::rVirRegister>(reg))
                        func(vir);
        }

        template<typename T>
        inline void for_each_use_vreg(T &&func) { for_each_vreg<false, true>(func); }

        template<typename T>
        inline void for_each_def_vreg(T &&func) { for_each_vreg<true, false>(func); }

        template<bool visitDef = true, bool visitUse = true, typename T>
        inline void for_each_preg(T &&func) {
            if constexpr (visitDef)
                for (auto reg: regDef)
                    if (auto phy = dynamic_cast<mips::rPhyRegister>(reg))
                        func(phy);
            if constexpr (visitUse)
                for (auto reg: regUse)
                    if (auto phy = dynamic_cast<mips::rPhyRegister>(reg))
                        func(phy);
        }

        template<typename T>
        inline void for_each_use_preg(T &&func) { for_each_preg<false, true>(func); }

        template<typename T>
        inline void for_each_def_preg(T &&func) { for_each_preg<true, false>(func); }

        [[nodiscard]] inline bool isFuncCall() const {
            return ty == Ty::JAL || ty == Ty::JALR || ty == Ty::BGEZAL || ty == Ty::BLTZAL;
        }

        [[nodiscard]] inline bool isJumpBranch() const {
            return ty >= Ty::BEQ && ty <= Ty::JALR;
        }

        [[nodiscard]] inline bool isConditionalBranch() const {
            return ty >= Ty::BEQ && ty <= Ty::BLTZ;
        }

        [[nodiscard]] inline bool isUnconditionalJump() const {
            return ty >= Ty::J && ty <= Ty::JR;
        }

        [[nodiscard]] virtual inline rLabel getJumpLabel() const { return nullptr; }

        virtual inline void setJumpLabel(rLabel newLabel) {}

        friend inline std::ostream &operator<<(std::ostream &os, Instruction::Ty t) {
            return os << magic_enum::enum_to_string_lower(t);
        }

        virtual inline std::ostream &output(std::ostream &os) const {
            return os << ty;
        }

        [[nodiscard]] virtual inline pInstruction clone() const = 0;
    };

    template<typename T>
    struct InstructionImpl : Instruction {
        using Instruction::Instruction;

        [[nodiscard]] inline pInstruction clone() const override {
            return std::make_unique<T>(static_cast<const T &>(*this));
        }
    };

    struct BinaryRInst : InstructionImpl<BinaryRInst> {
        explicit BinaryRInst(Ty ty, rRegister dst, rRegister src1, rRegister src2)
                : InstructionImpl{ty, {dst}, {src1, src2}} {}

        explicit BinaryRInst(Ty ty, rRegister r1, rRegister r2) : InstructionImpl{ty} {
            if (ty == Ty::CLO || ty == Ty::CLZ) reg_def_push_back(r1), reg_use_push_back(r2);
            else reg_use_push_back(r1), reg_use_push_back(r2);
        }

        [[nodiscard]] inline rRegister dst() const { return regDef.empty() ? nullptr : regDef[0]; }

        [[nodiscard]] inline rRegister src1() const { return regUse.empty() ? nullptr : regUse[0]; }

        [[nodiscard]] inline rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }

        inline std::ostream &output(std::ostream &os) const override {
            bool first = true;
            os << ty << "\t";
            if (dst()) os << dst(), first = false;
            if (src1()) os << (first ? "" : ", ") << src1(), first = false;
            if (src2()) os << (first ? "" : ", ") << src2();
            return os;
        }
    };

    struct BinaryIInst : InstructionImpl<BinaryIInst> {
        pImmediate imm;

        BinaryIInst(const BinaryIInst &inst)
                : InstructionImpl(inst), imm(new Immediate(inst.imm->value)) {}

        explicit BinaryIInst(Ty ty, rRegister dst, rRegister src, int imm)
                : InstructionImpl{ty, {dst}, {src}}, imm(new Immediate(imm)) {}

        explicit BinaryIInst(Ty ty, rRegister dst, int imm)
                : InstructionImpl{ty, {dst}, {}}, imm(new Immediate(imm)) {}

        [[nodiscard]] inline rRegister dst() const { return regDef[0]; }

        [[nodiscard]] inline rRegister src() const { return regUse.empty() ? nullptr : regUse[0]; }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t" << dst();
            if (src()) os << ", " << src();
            os << ", " << imm;
            return os;
        }
    };

    struct LoadInst : InstructionImpl<LoadInst> {
        rLabel label;
        pImmediate offset;

        LoadInst(const LoadInst &inst)
                : InstructionImpl(inst), label(inst.label), offset(new Immediate(inst.offset->value)) {}

        LoadInst(LoadInst &&inst) = default;

        explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset)
                : InstructionImpl{ty, {dst}, {base}}, label(nullptr), offset(new Immediate(offset)) {}

        explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset, rLabel label)
                : InstructionImpl{ty, {dst}, {base}}, label(label), offset(new Immediate(offset)) {}

        explicit LoadInst(Ty ty, rRegister dst, rLabel label)
                : InstructionImpl{ty, {dst}, {}}, label(label), offset(nullptr) {}

        explicit LoadInst(Ty ty, rRegister dst, rAddress address)
                : LoadInst(ty, dst, address->base, address->offset, address->label) {}

        [[nodiscard]] inline rRegister dst() const { return regDef[0]; }

        [[nodiscard]] inline rRegister base() const { return regUse[0]; }

        inline std::ostream &output(std::ostream &os) const override {
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

        StoreInst(StoreInst &&inst) = default;

        explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset)
                : InstructionImpl{ty, {}, {src, base}}, label(nullptr), offset(new Immediate(offset)) {}

        explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset, rLabel label)
                : InstructionImpl{ty, {}, {src, base}}, label(label), offset(new Immediate(offset)) {}

        explicit StoreInst(Ty ty, rRegister src, rLabel label)
                : InstructionImpl{ty, {}, {src}}, label(label), offset(nullptr) {}

        explicit StoreInst(Ty ty, rRegister src, rAddress address)
                : StoreInst(ty, src, address->base, address->offset, address->label) {}

        [[nodiscard]] inline rRegister src() const { return regUse[0]; }

        [[nodiscard]] inline rRegister base() const { return regUse[1]; }

        inline std::ostream &output(std::ostream &os) const override {
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

        [[nodiscard]] inline rRegister src1() const { return regUse.empty() ? nullptr : regUse[0]; }

        [[nodiscard]] inline rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }

        [[nodiscard]] inline rLabel getJumpLabel() const override { return label; }

        inline void setJumpLabel(rLabel newLabel) override { this->label = newLabel; }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t" << src1() << ", ";
            if (src2()) os << src2() << ", ";
            os << label;
            return os;
        }
    };

    struct MoveInst : InstructionImpl<MoveInst> {
        explicit MoveInst(rRegister dst, rRegister src) : InstructionImpl{Ty::MOVE, {dst}, {src}} {}

        explicit MoveInst(Ty ty, rRegister universal) : InstructionImpl{ty} {
            if (ty == Ty::MFHI || ty == Ty::MFLO) reg_def_push_back(universal);
            else reg_use_push_back(universal);
        }

        [[nodiscard]] inline rRegister dst() const { return regDef.empty() ? nullptr : regDef[0]; }

        [[nodiscard]] inline rRegister src() const { return regUse.empty() ? nullptr : regUse[0]; }

        inline std::ostream &output(std::ostream &os) const override {
            if (ty == Ty::MOVE) {
                return os << ty << "\t" << dst() << ", " << src();
            } else {
                return os << ty << "\t" << (dst() ? dst() : src());
            }
        }
    };

    struct JumpInst : InstructionImpl<JumpInst> {
        rLabel label;

        explicit JumpInst(Ty ty, rLabel label)
                : InstructionImpl{ty}, label(label) {}

        explicit JumpInst(Ty ty, rRegister tar)
                : InstructionImpl{ty, {}, {tar}}, label(nullptr) {}

        explicit JumpInst(Ty ty, rRegister ra, rRegister tar)
                : InstructionImpl{ty, {}, {ra, tar}}, label(nullptr) {}

        [[nodiscard]] inline rRegister target() const { return regUse.empty() ? nullptr : regUse.back(); }

        [[nodiscard]] inline rRegister ra() const { return regUse.size() <= 1 ? nullptr : regUse.front(); }

        [[nodiscard]] inline rLabel getJumpLabel() const override { return label; }

        inline void setJumpLabel(rLabel newLabel) override { this->label = newLabel; }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t";
            if (label) os << label;
            else if (ra()) os << ra() << ", " << target();
            else os << target();
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

        static inline std::pair<pBinaryIInst, pSyscallInst> syscall(SyscallId id) {
            pBinaryIInst bin{new BinaryIInst(Ty::LI, PhyRegister::get("$v0"), static_cast<int>(id))};
            pSyscallInst sys{new SyscallInst()};
            sys->reg_use_push_back(PhyRegister::get("$v0"));
            switch (id) {
                case SyscallId::PrintInteger:
                case SyscallId::PrintString:
                case SyscallId::PrintCharacter:
                    sys->reg_use_push_back(PhyRegister::get("$a0"));
                    break;
                case SyscallId::ReadInteger:
                    sys->reg_def_push_back(PhyRegister::get("$v0"));
                    break;
                case SyscallId::ExitProc:
                    break;
            }
            return {std::move(bin), std::move(sys)};
        }

    private:
        explicit SyscallInst() : InstructionImpl{Ty::SYSCALL} {}
    };

    inline std::ostream &operator<<(std::ostream &os, const Instruction &inst) {
        return inst.output(os);
    }
}

#ifdef DBG_ENABLE
namespace dbg {
    inline std::string instruction_to_string(const mips::Instruction &value) {
        std::stringstream ss;
        ss << value;
        auto s = ss.str();
        for (auto &ch: s)
            if (ch == '\t') ch = ' ';
        return '"' + s + '"';
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::rInstruction &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << instruction_to_string(*value);
        return true;
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::pInstruction &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << instruction_to_string(*value);
        return true;
    }
}
#endif //DBG_ENABLE

#endif //COMPILER_MIPS_INSTRUCTION_H
