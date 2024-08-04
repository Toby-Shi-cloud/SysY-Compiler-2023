//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_RISCV_ALIAS_H
#define COMPILER_RISCV_ALIAS_H

#define ALIAS_DEF
#include "backend/alias.h"  // IWYU pragma: export

namespace backend::riscv {
def(Immediate);
def(IntImmediate);
def(LabelImmediate);
def(JoinImmediate);
def(Address);

def(PhyRegister);
def(XPhyRegister);
def(FPhyRegister);

def(Instruction);
def(RInstruction);
def(IInstruction);
def(SInstruction);
def(BInstruction);
def(UInstruction);
def(JInstruction);
}  // namespace backend::riscv

#undef ALIAS_DEF
#undef def

#endif  // COMPILER_RISCV_ALIAS_H
