//
// Created by toby on 2023/11/7.
//

#ifndef COMPILER_MIPS_ALIAS_H
#define COMPILER_MIPS_ALIAS_H

#define ALIAS_DEF
#include "backend/alias.h"

namespace backend::mips {
using magic_enum::lowercase::operator<<;
def(PhyRegister);
def(Instruction);
def(BinaryRInst);
def(BinaryIInst);
def(BinaryMInst);
def(LoadInst);
def(StoreInst);
def(BranchInst);
def(MoveInst);
def(JumpInst);
def(SyscallInst);
}  // namespace backend::mips

#undef ALIAS_DEF
#undef def

#endif  // COMPILER_MIPS_ALIAS_H
