//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_INSTRUCTION_H
#define COMPILER_MIPS_INSTRUCTION_H

namespace mips {
    struct Instruction {
        enum class Ty {
            NOP, ADDU, SUBU, MUL, AND, OR, NOR, XOR, SLLV, SRAV, SRLV, SLT, SLTU, MOVN, MOVZ,
            MULT, MULTU, MADD, MADDU, MSUB, MSUBU, DIV, DIVU, CLO, CLZ,
            ADDIU, ANDI, ORI, XORI, SLL, SRL, SRA, SLTI, SLTIU,
            LW, LWL, LWR, SW, SWL, SWR, LB, LH, LHU, LBU, SB, SH, LUI,
            BEQ, BNE, BGEZ, BGEZAL, BGTZ, BLEZ, BLTZ, BLTZAL,
            MFHI, MFLO, MTHI, MTLO, J, JR, JAL, JALR, SYSCALL
        } ty;
    };
}

#endif //COMPILER_MIPS_INSTRUCTION_H
