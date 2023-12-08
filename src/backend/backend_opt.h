//
// Created by toby on 2023/11/16.
//

#ifndef COMPILER_BACKEND_OPT_H
#define COMPILER_BACKEND_OPT_H

#include "../mips.h"

namespace backend {
    // Clear instructions that are translated but not used
    void clearDeadCode(mips::rFunction function);

    // Relocate all blocks to reduce the number of jumps
    void relocateBlock(mips::rFunction function);

    // Flod div & rem with same operands. (i.e. a/b, a%b can be flodded into one div instruction)
    void divisionFold(mips::rFunction function);

    // Convert x / imm (or x % imm) to multiplication (if possible)
    void div2mul(mips::rFunction function);

    // Clear duplicate instructions (LI/LUI)
    void clearDuplicateInst(mips::rFunction function);

    // Do some arithmetic folding
    void arithmeticFolding(mips::rFunction function);
}

#endif //COMPILER_BACKEND_OPT_H
