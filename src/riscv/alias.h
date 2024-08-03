//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_RISCV_ALIAS_H
#define COMPILER_RISCV_ALIAS_H

#define ALIAS_DEF
#include "backend/alias.h"

namespace backend::riscv {
using magic_enum::lowercase::operator<<;
def(PhyRegister);
def(XPhyRegister);
def(FPhyRegister);
def(Instruction);
}

#undef ALIAS_DEF
#undef def

#endif  // COMPILER_RISCV_ALIAS_H
