//
// Created by toby on 2023/11/7.
//

#ifndef COMPILER_BACKEND_ALIAS_H
#define COMPILER_BACKEND_ALIAS_H

#include <list>
#include <memory>
#include "enum.h"

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
namespace backend {
def(Operand);
def(Register);
def(Label);
def(Immediate);
def(DynImmediate);
def(VirRegister);
def(Address);
}  // namespace backend

namespace backend {
def(InstructionBase);
def(SubBlock);
def(Block);
def(Function);
def(GlobalVar);
def(Module);
}  // namespace backend

#undef def

namespace backend {
using inst_node_t = std::list<pInstructionBase>::iterator;
using inst_pos_t = std::list<pInstructionBase>::const_iterator;
using block_node_t = std::list<pBlock>::iterator;
using block_pos_t = std::list<pBlock>::const_iterator;

template <typename T>
std::unique_ptr<T> make_copy(const std::unique_ptr<T> &ptr) {
    if (ptr == nullptr) return nullptr;
    return ptr->clone();
}
}  // namespace backend

#endif  // COMPILER_BACKEND_ALIAS_H
