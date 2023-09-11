//
// Created by toby on 2023/11/7.
//

#ifndef COMPILER_MIPS_ALIAS_H
#define COMPILER_MIPS_ALIAS_H

#include <memory>
#include "../dbg.h"

/**
 * @brief Define alias for pointer and reference of a type. <br>
 * p##T is alias for std::unique_ptr<T> (unique pointer T). <br>
 * r##T is alias for T * (raw pointer T). <br>
 * unique_ptr is used to manage memory, and raw pointer is used to access the object. <br>
 */
#define def(T) struct T; using p##T = std::unique_ptr<T>; using r##T = T *

// operand.h
namespace mips {
    def(Operand);
    def(Register);
    def(Label);
    def(Address);
    def(Immediate);
    def(PhyRegister);
    def(VirRegister);
}

// component.h
namespace mips {
    def(Block);
    def(Function);
    def(GlobalVar);
    def(Module);
}

// instruction.h
namespace mips {
    def(Instruction);
    def(BinaryRInst);
    def(BinaryIInst);
    def(LoadInst);
    def(StoreInst);
    def(BranchInst);
    def(MoveInst);
    def(JumpInst);
    def(SyscallInst);
}

#undef def

namespace mips {
    template<typename T>
    inline std::unique_ptr<T> make_copy(const std::unique_ptr<T> &ptr) {
        if (ptr == nullptr) return nullptr;
        return std::make_unique<T>(*ptr);
    }
}

#endif //COMPILER_MIPS_ALIAS_H
