//
// Created by toby on 2023/11/7.
//

#ifndef COMPILER_MIPS_ALIAS_H
#define COMPILER_MIPS_ALIAS_H

#include "backend/alias.h"

/**
 * @brief Define alias for pointer and reference of a type. <br>
 * p##T is alias for std::unique_ptr<T> (unique pointer T). <br>
 * r##T is alias for T * (raw pointer T). <br>
 * unique_ptr is used to manage memory, and raw pointer is used to access the object. <br>
 */
#define def(T)                       \
    struct T;                        \
    using p##T = std::unique_ptr<T>; \
    using r##T = T*

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

#undef def

#endif  // COMPILER_MIPS_ALIAS_H
