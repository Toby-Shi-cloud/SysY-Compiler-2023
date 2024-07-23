//
// Created by toby on 2023/11/7.
//

#ifndef COMPILER_MIPS_ALIAS_H
#define COMPILER_MIPS_ALIAS_H

#include <list>
#include <memory>
#include "../enum.h"

/**
 * @brief Define alias for pointer and reference of a type. <br>
 * p##T is alias for std::unique_ptr<T> (unique pointer T). <br>
 * r##T is alias for T * (raw pointer T). <br>
 * unique_ptr is used to manage memory, and raw pointer is used to access the object. <br>
 */
#define def(T)                       \
    struct T;                        \
    using p##T = std::unique_ptr<T>; \
    using r##T = T *

// operand.h
namespace mips {
using magic_enum::lowercase::operator<<;
def(Operand);
def(Register);
def(Label);
def(Immediate);
def(DynImmediate);
def(PhyRegister);
def(VirRegister);
def(Address);
}  // namespace mips

// component.h
namespace mips {
def(SubBlock);
def(Block);
def(Function);
def(GlobalVar);
def(Module);
}  // namespace mips

// instruction.h
namespace mips {
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
}  // namespace mips

#undef def

namespace mips {
using inst_node_t = std::list<pInstruction>::iterator;
using inst_pos_t = std::list<pInstruction>::const_iterator;
using block_node_t = std::list<pBlock>::iterator;
using block_pos_t = std::list<pBlock>::const_iterator;

template <typename T>
std::unique_ptr<T> make_copy(const std::unique_ptr<T> &ptr) {
    if (ptr == nullptr) return nullptr;
    return ptr->clone();
}
}  // namespace mips

#endif  // COMPILER_MIPS_ALIAS_H
