//
// Created by toby on 2024/8/5.
//

#ifndef COMPILER_RISCV_OPT_H
#define COMPILER_RISCV_OPT_H

#include "riscv/instruction.h"

namespace backend::riscv {
// Clear instructions that are translated but not used
void clearDeadCode(rFunction function);

// Merge empty blocks (only jump...)
void mergeBlocks(rFunction function);

// Relocate all blocks to reduce the number of jumps
void relocateBlock(rFunction function);

// Translate x * imm to shift (if possible)
rRegister process_mul(rBlock block, Instruction::Ty ty, rRegister reg, uint64_t mul);
// Translate x / imm & x % imm
rRegister process_div(rBlock block, rRegister reg, int32_t div);
rRegister process_divu(rBlock block, rRegister reg, uint32_t div);
rRegister process_rem(rBlock block, rRegister reg, int32_t div);
rRegister process_remu(rBlock block, rRegister reg, uint32_t div);

// Do some arithmetic folding
void arithmeticFolding(rFunction function);
}  // namespace backend::riscv

#endif  // COMPILER_RISCV_OPT_H
