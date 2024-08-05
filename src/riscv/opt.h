//
// Created by toby on 2024/8/5.
//

#ifndef COMPILER_RISCV_OPT_H
#define COMPILER_RISCV_OPT_H

#include "riscv/instruction.h"

namespace backend::riscv {
// Clear instructions that are translated but not used
void clearDeadCode(rFunction function);

// Relocate all blocks to reduce the number of jumps
void relocateBlock(rFunction function);

// Flod div & rem with same operands. (i.e. a/b, a%b can be flodded into one div instruction)
void divisionFold(rFunction function);

// Convert x / imm (or x % imm) to multiplication (if possible)
void div2mul(rFunction function);

// Do some arithmetic folding
void arithmeticFolding(rFunction function);
}  // namespace backend::riscv

#endif  // COMPILER_RISCV_OPT_H
