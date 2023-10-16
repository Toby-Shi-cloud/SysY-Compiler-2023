//
// Created by toby2 on 2023/10/12.
//

#ifndef COMPILER_INSTRUCTION_H
#define COMPILER_INSTRUCTION_H

#include "derived_value.h"
#include <sstream>

namespace mir {
    template<typename T>
    static inline decltype(std::stringstream{} << std::declval<T>(), std::string{})
    string_of(const T &value) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }

    struct Instruction::ret : Instruction {
        explicit ret() : Instruction(Type::getVoidType(), RET) {}

        explicit ret(Value *value) : Instruction(Type::getVoidType(), RET, value) {}

        [[nodiscard]] Value *getReturnValue() const {
            return getNumOperands() == 0 ? nullptr : getOperand(0);
        }

        [[nodiscard]] std::string to_string() const override;
    };

    struct Instruction::br : Instruction {
        explicit br(BasicBlock *target) : Instruction(Type::getVoidType(), BR, target) {}

        explicit br(Value *cond, BasicBlock *ifTrue, BasicBlock *ifFalse) :
                Instruction(Type::getVoidType(), BR, cond, ifTrue, ifFalse) {}

        [[nodiscard]] bool hasCondition() const { return getNumOperands() == 3; }

        [[nodiscard]] BasicBlock *getTarget() const {
            return hasCondition() ? nullptr : getOperand<BasicBlock>(0);
        }

        [[nodiscard]] Value *getCondition() const {
            return hasCondition() ? getOperand(0) : nullptr;
        }

        [[nodiscard]] BasicBlock *getIfTrue() const {
            return hasCondition() ? getOperand<BasicBlock>(1) : getOperand<BasicBlock>(0);
        }

        [[nodiscard]] BasicBlock *getIfFalse() const {
            return hasCondition() ? getOperand<BasicBlock>(2) : getOperand<BasicBlock>(0);
        }

        [[nodiscard]] std::string to_string() const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_binary_instruction : Instruction {
        explicit _binary_instruction(Value *lhs, Value *rhs) : Instruction(lhs->getType(), ty, lhs, rhs) {
            assert(lhs->getType() == rhs->getType());
        }

        [[nodiscard]] Value *getLhs() const { return getOperand(0); }

        [[nodiscard]] Value *getRhs() const { return getOperand(1); }

        [[nodiscard]] std::string to_string() const override {
            return getName() + " = " + string_of(ty) + " " + string_of(getLhs()) + ", " + getRhs()->getName();
        }
    };

    struct Instruction::alloca_ : Instruction {
        explicit alloca_(pType type) : Instruction(type, ALLOCA) {}

        [[nodiscard]] std::string to_string() const override;
    };

    struct Instruction::load : Instruction {
        explicit load(pType type, Value *ptr) : Instruction(type, LOAD, ptr) {}

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        [[nodiscard]] std::string to_string() const override;
    };

    struct Instruction::store : Instruction {
        explicit store(Value *src, Value *dest) : Instruction(Type::getVoidType(), STORE, src, dest) {}

        [[nodiscard]] Value *getSrc() const { return getOperand(0); }

        [[nodiscard]] Value *getDest() const { return getOperand(1); }

        [[nodiscard]] std::string to_string() const override;
    };

    struct Instruction::getelementptr : Instruction {
        explicit getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs);

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        [[nodiscard]] Value *getIndexOperand(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumIndices() const { return getNumOperands() - 1; }

        [[nodiscard]] std::string to_string() const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_conversion_instruction : Instruction {
        explicit _conversion_instruction(pType type, Value *value) : Instruction(type, ty, value) {}

        [[nodiscard]] Value *getValueOperand() const { return getOperand(0); }

        [[nodiscard]] std::string to_string() const override {
            return getName() + " = " + string_of(ty) + " " + string_of(getValueOperand()) + " to " + string_of(getType());
        }
    };

    struct Instruction::icmp : Instruction {
        enum Cond {
            EQ, NE, UGT, UGE, ULT, ULE, SGT, SGE, SLT, SLE
        } cond;

        explicit icmp(Cond cond, Value *lhs, Value *rhs) :
                Instruction(Type::getI1Type(), ICMP, lhs, rhs), cond(cond) {
            assert(lhs->getType() == rhs->getType());
            assert(lhs->getType()->isIntegerTy());
        }

        [[nodiscard]] Value *getLhs() const { return getOperand(0); }

        [[nodiscard]] Value *getRhs() const { return getOperand(1); }

        [[nodiscard]] std::string to_string() const override;
    };

    inline std::ostream &operator<<(std::ostream &os, Instruction::icmp::Cond cond) {
        static constexpr const char *str[] = {
                "eq", "ne", "ugt", "uge", "ult", "ule", "sgt", "sge", "slt", "sle"
        };
        return os << str[cond];
    }

    struct Instruction::phi : Instruction {
        explicit phi(const std::vector<Value *> &values) : Instruction(values[0]->getType(), PHI, values) {}

        [[nodiscard]] std::pair<Value *, BasicBlock *> getIncomingValue(int i) const {
            return {getOperand(i * 2), getOperand<BasicBlock>(i * 2 + 1)};
        }

        [[nodiscard]] size_t getNumIncomingValues() const { return getNumOperands() / 2; }

        [[nodiscard]] std::string to_string() const override;
    };

    struct Instruction::call : Instruction {
        explicit call(Function *func, const std::vector<Value *> &args);

        [[nodiscard]] Function *getFunction() const { return getOperand<Function>(0); }

        [[nodiscard]] Value *getArg(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumArgs() const { return getNumOperands() - 1; }

        [[nodiscard]] std::string to_string() const override;
    };
}

#endif //COMPILER_INSTRUCTION_H