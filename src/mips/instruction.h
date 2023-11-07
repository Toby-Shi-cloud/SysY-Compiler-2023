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
            MULT, MULTU, MADD, MADDU, MSUB, MSUBU, DIV, DIVU, CLO, CLZ,
            ADDIU, ANDI, ORI, XORI, SLL, SRL, SRA, SLTI, SLTIU, LUI, REM, REMU,
            LA, LW, LWL, LWR, LB, LH, LHU, LBU, SW, SWL, SWR, SB, SH,
            BEQ, BNE, BGEZ, BGTZ, BLEZ, BLTZ, BGEZAL, BLTZAL,
            MFHI, MFLO, MTHI, MTLO, J, JR, JAL, JALR, SYSCALL
        } ty;
        std::vector<rRegister> regDef, regUse;
        rBlock parent = nullptr;

        virtual ~Instruction() = default;

        explicit Instruction() : ty{Ty::NOP} {}

        explicit Instruction(Ty ty) : ty{ty} {}

        explicit Instruction(Ty ty, std::vector<rRegister> regDef, std::vector<rRegister> regUse)
                : ty{ty}, regDef{std::move(regDef)}, regUse{std::move(regUse)} {}

        explicit Instruction(Ty ty, std::vector<rRegister> regDef, std::vector<rRegister> regUse, rBlock parent)
                : ty{ty}, regDef{std::move(regDef)}, regUse{std::move(regUse)}, parent{parent} {}

        [[nodiscard]] inline bool isNop() const { return ty == Ty::NOP; }

        [[nodiscard]] inline bool isBinaryRInst() const { return ty >= Ty::ADDU && ty <= Ty::CLZ; }

        [[nodiscard]] inline bool isBinaryIInst() const { return ty >= Ty::ADDIU && ty <= Ty::LUI; }

        [[nodiscard]] inline bool isLoadInst() const { return ty >= Ty::LW && ty <= Ty::LBU; }

        [[nodiscard]] inline bool isStoreInst() const { return ty >= Ty::SW && ty <= Ty::SH; }

        [[nodiscard]] inline bool isBranchInst() const { return ty >= Ty::BEQ && ty <= Ty::BLTZAL; }

        [[nodiscard]] inline bool isMoveInst() const { return ty >= Ty::MFHI && ty <= Ty::MTLO; }

        [[nodiscard]] inline bool isJumpInst() const { return ty >= Ty::J && ty <= Ty::JALR; }

        [[nodiscard]] inline bool isSyscallInst() const { return ty == Ty::SYSCALL; }

        friend inline std::ostream &operator<<(std::ostream &os, Instruction::Ty t) {
            return os << magic_enum::enum_to_string_lower(t);
        }

        virtual inline std::ostream &output(std::ostream &os) const {
            return os << ty;
        }
    };

    struct BinaryRInst : Instruction {
        explicit BinaryRInst(Ty ty, rRegister dst, rRegister src1, rRegister src2)
                : Instruction{ty, {dst}, {src1, src2}} {}

        explicit BinaryRInst(Ty ty, rRegister r1, rRegister r2) : Instruction{ty} {
            if (ty == Ty::CLO || ty == Ty::CLZ) regDef.push_back(r1), regUse.push_back(r2);
            else regUse.push_back(r1), regUse.push_back(r2);
        }

        [[nodiscard]] inline rRegister dst() const { return regDef.empty() ? nullptr : regDef[0]; }

        [[nodiscard]] inline rRegister src1() const { return regUse.empty() ? nullptr : regUse[0]; }

        [[nodiscard]] inline rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }

        inline std::ostream &output(std::ostream &os) const override {
            bool first = true;
            os << ty << "\t";
            if (dst()) os << (first ? "" : ", ") << dst(), first = false;
            if (src1()) os << (first ? "" : ", ") << src1(), first = false;
            if (src2()) os << (first ? "" : ", ") << src2(), first = false;
            return os;
        }
    };

    struct BinaryIInst : Instruction {
        pImmediate imm;

        explicit BinaryIInst(Ty ty, rRegister dst, rRegister src, int imm)
                : Instruction{ty, {dst}, {src}}, imm(new Immediate(imm)) {}

        explicit BinaryIInst(Ty ty, rRegister dst, int imm)
                : Instruction{ty, {dst}, {}}, imm(new Immediate(imm)) {}

        [[nodiscard]] inline rRegister dst() const { return regDef[0]; }

        [[nodiscard]] inline rRegister src() const { return regUse.empty() ? nullptr : regUse[0]; }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t" << dst();
            if (src()) os << ", " << src();
            os << ", " << imm;
            return os;
        }
    };

    struct LoadInst : Instruction {
        rLabel label;
        pImmediate offset;

        explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset)
                : Instruction{ty, {dst}, {base}}, label(nullptr), offset(new Immediate(offset)) {}

        explicit LoadInst(Ty ty, rRegister dst, rRegister base, int offset, rLabel label)
                : Instruction{ty, {dst}, {base}}, label(label), offset(new Immediate(offset)) {}

        explicit LoadInst(Ty ty, rRegister dst, rLabel label)
                : Instruction{ty, {dst}, {}}, label(label), offset(nullptr) {}

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

    struct StoreInst : Instruction {
        rLabel label;
        pImmediate offset;

        explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset)
                : Instruction{ty, {}, {src, base}}, label(nullptr), offset(new Immediate(offset)) {}

        explicit StoreInst(Ty ty, rRegister src, rRegister base, int offset, rLabel label)
                : Instruction{ty, {}, {src, base}}, label(label), offset(new Immediate(offset)) {}

        explicit StoreInst(Ty ty, rRegister src, rLabel label)
                : Instruction{ty, {}, {src}}, label(label), offset(nullptr) {}

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

    struct BranchInst : Instruction {
        rLabel label;

        explicit BranchInst(Ty ty, rRegister src1, rRegister src2, rLabel label)
                : Instruction{ty, {}, {src1, src2}}, label(label) {}

        explicit BranchInst(Ty ty, rRegister src1, rLabel label)
                : Instruction{ty, {}, {src1}}, label(label) {}

        [[nodiscard]] inline rRegister src1() const { return regUse.empty() ? nullptr : regUse[0]; }

        [[nodiscard]] inline rRegister src2() const { return regUse.size() <= 1 ? nullptr : regUse[1]; }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t" << src1() << ", ";
            if (src2()) os << src2() << ", ";
            os << label;
            return os;
        }
    };

    struct MoveInst : Instruction {
        explicit MoveInst(Ty ty, rRegister universal) : Instruction{ty} {
            if (ty == Ty::MFHI || ty == Ty::MFLO) regDef.push_back(universal);
            else regUse.push_back(universal);
        }

        [[nodiscard]] inline rRegister dst() const { return regDef.empty() ? nullptr : regDef[0]; }

        [[nodiscard]] inline rRegister src() const { return regUse.empty() ? nullptr : regUse[0]; }

        inline std::ostream &output(std::ostream &os) const override {
            return os << ty << "\t" << (dst() ? dst() : src());
        }
    };

    struct JumpInst : Instruction {
        rLabel label;

        explicit JumpInst(Ty ty, rLabel label)
                : Instruction{ty}, label(label) {}

        explicit JumpInst(Ty ty, rRegister tar)
                : Instruction{ty, {}, {tar}}, label(nullptr) {}

        explicit JumpInst(Ty ty, rRegister ra, rRegister tar)
                : Instruction{ty, {}, {ra, tar}}, label(nullptr) {}

        [[nodiscard]] inline rRegister target() const { return regUse.empty() ? nullptr : regUse.back(); }

        [[nodiscard]] inline rRegister ra() const { return regUse.size() <= 1 ? nullptr : regUse.front(); }

        inline std::ostream &output(std::ostream &os) const override {
            os << ty << "\t";
            if (label) os << label;
            else if (ra()) os << ra() << ", " << target();
            else os << target();
            return os;
        }
    };

    struct SyscallInst : Instruction {
        enum class SyscallId : unsigned {
            PrintInteger = 1,
            PrintString = 4,
            ReadInteger = 5,
            ExitProc = 10,
            PrintCharacter = 11,
        } id;

        explicit SyscallInst(SyscallId id) : Instruction{Ty::SYSCALL}, id(id) {}

        inline std::ostream &output(std::ostream &os) const override {
            os << "ori\t$v0, $zero, " << static_cast<unsigned>(id) << std::endl;
            return os << "\t" << ty;
        }
    };

    inline std::ostream &operator<<(std::ostream &os, const Instruction &inst) {
        return inst.output(os);
    }
}

#endif //COMPILER_MIPS_INSTRUCTION_H
