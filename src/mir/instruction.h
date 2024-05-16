//
// Created by toby on 2023/10/12.
//

#ifndef COMPILER_MIR_INSTRUCTION_H
#define COMPILER_MIR_INSTRUCTION_H

#include <sstream>
#include <algorithm>
#include "derived_value.h"

namespace mir {
    struct Instruction::ret : Instruction {
        explicit ret() : Instruction(Type::getVoidType(), RET) {}

        explicit ret(Value *value) : Instruction(Type::getVoidType(), RET, value) {}

        static ret *default_t(pType ty) {
            if (ty->isVoidTy()) return new ret();
            if (ty->isFloatTy()) return new ret(getFloatLiteral(0));
            if (ty->isIntegerTy()) return new ret(getIntegerLiteral(0));
            throw std::runtime_error("unknown type for ret");
        }

        [[nodiscard]] Value *getReturnValue() const {
            return getNumOperands() == 0 ? nullptr : getOperand(0);
        }

        [[nodiscard]] Instruction *clone() const override { return new ret(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.lastBB = interpreter.currentBB;
            interpreter.currentBB = nullptr;
            interpreter.retValue = getReturnValue() ? interpreter.getValue(getReturnValue()) : 0;
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

        [[nodiscard]] Instruction *clone() const override { return new br(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.lastBB = interpreter.currentBB;
            if (hasCondition()) {
                if (interpreter.getValue(getCondition())) interpreter.currentBB = getIfTrue();
                else interpreter.currentBB = getIfFalse();
            } else {
                interpreter.currentBB = getTarget();
            }
        }

        std::ostream &output(std::ostream &os) const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_binary_instruction : Instruction {
        static_assert(ty >= ADD && ty <= XOR);

        explicit _binary_instruction(Value *lhs, Value *rhs) : Instruction(lhs->getType(), ty, lhs, rhs) {
            assert(lhs->getType() == rhs->getType());
        }

        [[nodiscard]] Value *getLhs() const { return getOperand(0); }

        [[nodiscard]] Value *getRhs() const { return getOperand(1); }

        void interpret(Interpreter &interpreter) const override {
            constexpr auto calcFunc = [] (unsigned lhs, unsigned rhs) {
                if constexpr (ty == ADD) return lhs + rhs;
                if constexpr (ty == SUB) return lhs - rhs;
                if constexpr (ty == MUL) return lhs * rhs;
                if constexpr (ty == UDIV) return lhs / rhs;
                if constexpr (ty == SDIV) return (int) lhs / (int) rhs;
                if constexpr (ty == UREM) return lhs % rhs;
                if constexpr (ty == SREM) return (int) lhs % (int) rhs;
                if constexpr (ty == FADD) return lhs + rhs;
                if constexpr (ty == FSUB) return lhs - rhs;
                if constexpr (ty == FMUL) return lhs * rhs;
                if constexpr (ty == FDIV) return lhs / rhs;
                if constexpr (ty == FREM) return lhs % rhs;
                if constexpr (ty == SHL) return lhs << rhs;
                if constexpr (ty == LSHR) return lhs >> rhs;
                if constexpr (ty == ASHR) return (int) lhs >> (int) rhs;
                if constexpr (ty == AND) return lhs & rhs;
                if constexpr (ty == OR) return lhs | rhs;
                if constexpr (ty == XOR) return lhs ^ rhs;
                __builtin_unreachable();
            };
            interpreter.map[this] = calcFunc(interpreter.getValue(getLhs()), interpreter.getValue(getRhs()));
        }

        [[nodiscard]] IntegerLiteral *calc() const {
            Interpreter interpreter{};
            interpret(interpreter);
            return getIntegerLiteral(interpreter.getValue(this));
        }

        [[nodiscard]] std::vector<Value *> getOperands() const override {
            auto res = User::getOperands();
            if constexpr (ty == ADD || ty == MUL || ty == AND || ty == OR || ty == XOR)
                std::sort(res.begin(), res.end());
            return res;
        }

        [[nodiscard]] Instruction *clone() const override { return new _binary_instruction(*this); }

        std::ostream &output(std::ostream &os) const override {
            return os << name << " = " << ty << " " << getLhs() << ", " << getRhs()->name;
        }
    };

    struct Instruction::fneg : Instruction {
        explicit fneg(Value *value) : Instruction(value->getType(), FNEG, value) {
            assert(value->getType()->isFloatTy());
        }

        [[nodiscard]] auto getOperand() const { return Instruction::getOperand(0); }

        [[nodiscard]] Instruction *clone() const override { return new fneg(*this); }

        void interpret(Interpreter &interpreter) const override { /*todo...*/ }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::alloca_ : Instruction {
        explicit alloca_(pType type) : Instruction(type, ALLOCA) {}

        [[nodiscard]] Instruction *clone() const override { return new alloca_(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.map[this] = (int) interpreter.stack.size();
            interpreter.stack.resize(interpreter.stack.size() + getType()->size() / 4);
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::load : Instruction {
        explicit load(pType type, Value *ptr) : Instruction(type, LOAD, ptr) {}

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        [[nodiscard]] Instruction *clone() const override { return new load(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.map[this] = interpreter.stack.at(interpreter.getValue(getPointerOperand()));
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::store : Instruction {
        explicit store(Value *src, Value *dest) : Instruction(Type::getVoidType(), STORE, src, dest) {}

        [[nodiscard]] Value *getSrc() const { return getOperand(0); }

        [[nodiscard]] Value *getDest() const { return getOperand(1); }

        [[nodiscard]] Instruction *clone() const override { return new store(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.stack[interpreter.getValue(getDest())] = interpreter.getValue(getSrc());
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::getelementptr : Instruction {
        pType indexTy;

        explicit getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs);

        [[nodiscard]] Value *getPointerOperand() const { return getOperand(0); }

        [[nodiscard]] Value *getIndexOperand(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumIndices() const { return getNumOperands() - 1; }

        [[nodiscard]] static pType getIndexTy(pType pointerOperandType) {
            auto index_ty = pointerOperandType;
            if (index_ty->isPointerTy()) index_ty = index_ty->getPointerBase();
            return index_ty;
        }

        [[nodiscard]] Instruction *clone() const override { return new getelementptr(*this); }

        [[nodiscard]] int getIndexOffset(const Interpreter &interpreter = {}) const {
            int curPos = 0;
            auto curType = indexTy;
            for (int i = 0; i < getNumIndices(); i++) {
                if (i) curType = curType->getBase();
                curPos += (int) curType->size() / 4 * interpreter.getValue(getIndexOperand(i));
            }
            return curPos;
        }

        void interpret(Interpreter &interpreter) const override {
            interpreter.map[this] = interpreter.getValue(getPointerOperand()) + getIndexOffset(interpreter);
        }

        std::ostream &output(std::ostream &os) const override;
    };

    template<Instruction::InstrTy ty>
    struct Instruction::_conversion_instruction : Instruction {
        static_assert(ty >= TRUNC && ty <= SITOFP);

        explicit _conversion_instruction(pType type, Value *value) : Instruction(type, ty, value) {}

        [[nodiscard]] Value *getValueOperand() const { return getOperand(0); }

        [[nodiscard]] Instruction *clone() const override { return new _conversion_instruction(*this); }

        void interpret(Interpreter &interpreter) const override {
            interpreter.map[this] = interpreter.getValue(getValueOperand());
        }

        std::ostream &output(std::ostream &os) const override {
            return os << name << " = " << ty << " " << getValueOperand() << " to " << getType();
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

        [[nodiscard]] Instruction *clone() const override { return new icmp(*this); }

        void interpret(Interpreter &interpreter) const override {
            auto calc = [this](int lhs, int rhs) {
                switch (cond) {
                    case EQ: return lhs == rhs;
                    case NE: return lhs != rhs;
                    case UGT: return (unsigned) lhs > (unsigned) rhs;
                    case UGE: return (unsigned) lhs >= (unsigned) rhs;
                    case ULT: return (unsigned) lhs < (unsigned) rhs;
                    case ULE: return (unsigned) lhs <= (unsigned) rhs;
                    case SGT: return lhs > rhs;
                    case SGE: return lhs >= rhs;
                    case SLT: return lhs < rhs;
                    case SLE: return lhs <= rhs;
                    default: __builtin_unreachable();
                }
            };
            interpreter.map[this] = calc(interpreter.getValue(getLhs()), interpreter.getValue(getRhs()));
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::fcmp : Instruction {
        enum Cond {
            FALSE,
            OEQ, OGT, OGE, OLT, OLE, ONE, ORD,
            UEQ, UGT, UGE, ULT, ULE, UNE, UNO,
            TRUE,
        } cond;

        explicit fcmp(Cond cond, Value *lhs, Value *rhs) :
                Instruction(Type::getI1Type(), FCMP, lhs, rhs), cond(cond) {
            assert(lhs->getType() == rhs->getType());
            assert(lhs->getType()->isFloatTy());
        }

        [[nodiscard]] Value *getLhs() const { return getOperand(0); }

        [[nodiscard]] Value *getRhs() const { return getOperand(1); }

        [[nodiscard]] Instruction *clone() const override { return new fcmp(*this); }

        void interpret(Interpreter &interpreter) const override { /*todo!*/ }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::phi : Instruction {
        using incominng_pair = std::pair<Value *, BasicBlock *>;

        explicit phi(pType type) : Instruction(type, PHI) {}

        explicit phi(const std::vector<incominng_pair> &values);

        void addIncomingValue(const incominng_pair &pair) {
            addOperand(pair.first);
            addOperand(pair.second);
        }

        void eraseIncomingValue(const BasicBlock *bb) {
            auto where = findOperand(bb);
            assert(where & 1);
            eraseOperand(where - 1, where + 1);
        }

        void substituteValue(const BasicBlock *which, Value *_new) {
            substituteOperand(findOperand(which) - 1, _new);
        }

        [[nodiscard]] incominng_pair getIncomingValue(const BasicBlock *bb) const {
            auto where = findOperand(bb);
            return {getOperand(where - 1), getOperand<BasicBlock>(where)};
        }

        [[nodiscard]] incominng_pair getIncomingValue(int i) const {
            return {getOperand(i * 2), getOperand<BasicBlock>(i * 2 + 1)};
        }

        [[nodiscard]] size_t getNumIncomingValues() const { return getNumOperands() / 2; }

        // check if instruction phi is valid. (may use under debug)
        [[nodiscard, maybe_unused]] bool checkValid() const {
            auto check_set = parent->predecessors;
            for (auto i = 0; i < getNumIncomingValues(); i++) {
                auto [value, bb] = getIncomingValue(i);
                if (value->getType() != getType()) return false;
                if (check_set.count(bb)) check_set.erase(bb);
                else return false;
            }
            return check_set.empty();
        }

        [[nodiscard]] Instruction *clone() const override { return new phi(*this); }

        void interpret(Interpreter &interpreter) const override {
            // phi instruction will write value in temp map, and will be copied to map after all phis are interpreted.
            interpreter.phi[this] = interpreter.getValue(getIncomingValue(interpreter.lastBB).first);
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::select : Instruction {
        explicit select(Value *cond, Value *ifTrue, Value *ifFalse) :
            Instruction(ifTrue->getType(), SELECT, cond, ifTrue, ifFalse) {
            assert(cond->getType() == Type::getI1Type());
            assert(ifTrue->getType() == ifFalse->getType());
        }

        [[nodiscard]] Value *getCondition() const { return getOperand(0); }

        [[nodiscard]] Value *getTrueValue() const { return getOperand(1); }

        [[nodiscard]] Value *getFalseValue() const { return getOperand(2); }

        [[nodiscard]] Instruction *clone() const override { return new select(*this); }

        void interpret(Interpreter &interpreter) const override {
            if (interpreter.getValue(getCondition())) interpreter.map[this] = interpreter.getValue(getTrueValue());
            else interpreter.map[this] = interpreter.getValue(getFalseValue());
        }

        std::ostream &output(std::ostream &os) const override;
    };

    struct Instruction::call : Instruction {
        explicit call(Function *func, const std::vector<Value *> &args);

        [[nodiscard]] Function *getFunction() const { return getOperand<Function>(0); }

        [[nodiscard]] Value *getArg(int i) const { return getOperand(i + 1); }

        [[nodiscard]] size_t getNumArgs() const { return getNumOperands() - 1; }

        [[nodiscard]] Instruction *clone() const override { return new call(*this); }

        void interpret(Interpreter &interpreter) const override {
            std::vector<int> args;
            for (int i = 0; i < getNumArgs(); i++)
                args.push_back(interpreter.getValue(getArg(i)));
            interpreter.map[this] = getFunction()->interpret(args);
        }

        std::ostream &output(std::ostream &os) const override;
    };
}

#endif //COMPILER_MIR_INSTRUCTION_H
