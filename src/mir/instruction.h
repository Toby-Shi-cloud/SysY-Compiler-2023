//
// Created by toby2 on 2023/10/12.
//

#ifndef COMPILER_MIR_INSTRUCTION_H
#define COMPILER_MIR_INSTRUCTION_H

#include <sstream>
#include "../enum.h"
#include "derived_value.h"

namespace mir {
    struct Instruction::ret : Instruction {
        explicit ret() : Instruction(Type::getVoidType(), RET) {}

        explicit ret(Value *value) : Instruction(Type::getVoidType(), RET, value) {}

        [[nodiscard]] Value *getReturnValue() const {
            return getNumOperands() == 0 ? nullptr : getOperand(0);
        }

        std::ostream &output(std::ostream &os) const override;
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

        std::ostream &output(std::ostream &os) const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_binary_instruction : Instruction {
        explicit _binary_instruction(Value *lhs, Value *rhs) : Instruction(lhs->getType(), ty, lhs, rhs) {
            assert(lhs->getType() == rhs->getType());
        }

        [[nodiscard]] Value *getLhs() const { return getOperand(0); }

        [[nodiscard]] Value *getRhs() const { return getOperand(1); }

        std::ostream &output(std::ostream &os) const override {
            return os << getName() << " = " << ty << " " << getLhs() << ", " << getRhs()->getName();
        }
    };

    struct Instruction::alloca_ : Instruction {
        explicit alloca_(pType type) : Instruction(type, ALLOCA) {}

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::load : Instruction {
        explicit load(pType type, Value *ptr) : Instruction(type, LOAD, ptr) {}

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::store : Instruction {
        explicit store(Value *src, Value *dest) : Instruction(Type::getVoidType(), STORE, src, dest) {}

        [[nodiscard]] Value *getSrc() const { return getOperand(0); }

        [[nodiscard]] Value *getDest() const { return getOperand(1); }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::getelementptr : Instruction {
        explicit getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs);

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        [[nodiscard]] Value *getIndexOperand(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumIndices() const { return getNumOperands() - 1; }

        [[nodiscard]] pType getIndexTy() const {
            auto index_ty = getPointerOperand()->getType();
            if (index_ty->isPointerTy()) index_ty = index_ty->getPointerBase();
            return index_ty;
        }

        std::ostream &output(std::ostream &os) const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_conversion_instruction : Instruction {
        explicit _conversion_instruction(pType type, Value *value) : Instruction(type, ty, value) {}

        [[nodiscard]] Value *getValueOperand() const { return getOperand(0); }

        std::ostream &output(std::ostream &os) const override {
            return os << getName() << " = " << ty << " " << getValueOperand() << " to " << getType();
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

        std::ostream &output(std::ostream &os) const override;
    };

    inline std::ostream &operator<<(std::ostream &os, Instruction::icmp::Cond cond) {
        return os << magic_enum::enum_to_string_lower(cond);
    }

    struct Instruction::phi : Instruction {
        using incominng_pair = std::pair<Value *, BasicBlock *>;

        explicit phi(pType type) : Instruction(type, PHI) {}

        explicit phi(const std::vector<incominng_pair> &values);

        void addIncomingValue(const incominng_pair &pair) {
            addOperand(pair.first);
            addOperand(pair.second);
        }

        [[nodiscard]] incominng_pair getIncomingValue(int i) const {
            return {getOperand(i * 2), getOperand<BasicBlock>(i * 2 + 1)};
        }

        [[nodiscard]] size_t getNumIncomingValues() const { return getNumOperands() / 2; }

        // check if instruction phi is valid. (may use under debug)
        [[nodiscard]] bool checkValid() const {
            auto check_set = parent->predecessors;
            for (auto i = 0; i < getNumIncomingValues(); i++) {
                auto [value, bb] = getIncomingValue(i);
                if (value->getType() != getType()) return false;
                if (check_set.count(bb)) check_set.erase(bb);
                else return false;
            }
            return check_set.empty();
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::call : Instruction {
        explicit call(Function *func, const std::vector<Value *> &args);

        [[nodiscard]] Function *getFunction() const { return getOperand<Function>(0); }

        [[nodiscard]] Value *getArg(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumArgs() const { return getNumOperands() - 1; }

        std::ostream &output(std::ostream &os) const override;
    };
}

#endif //COMPILER_MIR_INSTRUCTION_H
