//
// Created by toby2 on 2023/10/12.
//

#include "instruction.h"

namespace mir {
    static inline std::vector<Value *> merge(Value *ptr, const std::vector<Value *> &other) {
        std::vector<Value *> args = {ptr};
        args.insert(args.end(), other.begin(), other.end());
        return args;
    }

    std::string Instruction::ret::to_string() const {
        if (auto value = getReturnValue()) {
            return "ret " + string_of(value);
        } else {
            return "ret void";
        }
    }

    std::string Instruction::br::to_string() const {
        if (hasCondition()) {
            assert(getCondition()->getType() == (pType) Type::getI1Type());
            return "br " + string_of(getCondition()) + ", " + string_of(getIfTrue()) + ", " + string_of(getIfFalse());
        } else {
            return "br " + string_of(getTarget());
        }
    }

    std::string Instruction::alloca_::to_string() const {
        return getName() + " = alloca " + string_of(getType()) + ", align 4";
    }

    std::string Instruction::load::to_string() const {
        return getName() + " = load " + string_of(getType()) + ", ptr " + getPointerOperand()->getName();
    }

    std::string Instruction::store::to_string() const {
        return "store " + string_of(getSrc()) + ", ptr " + getDest()->getName();
    }

    Instruction::getelementptr::getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs)
            : Instruction(type, GETELEMENTPTR, merge(ptr, idxs)) {}

    std::string Instruction::getelementptr::to_string() const {
        std::stringstream ss;
        ss << getName() << " = getelementptr " << getPointerOperand()->getType()
           << ", ptr " << getPointerOperand()->getName();
        for (int i = 0; i < getNumIndices(); i++) {
            ss << ", " << getIndexOperand(i);
        }
        return ss.str();
    }

    std::string Instruction::icmp::to_string() const {
        return getName() + " = icmp " + string_of(cond) + " " + string_of(getLhs()) + ", " + getRhs()->getName();
    }

    std::string Instruction::phi::to_string() const {
        std::stringstream ss;
        ss << getName() << " = phi " << getType() << " ";
        for (int i = 0; i < getNumIncomingValues(); i++) {
            if (i) ss << ", ";
            auto [value, label] = getIncomingValue(i);
            ss << "[ " << value->getName() << ", " << label->getName() << " ]";
        }
        return ss.str();
    }

    Instruction::call::call(Function *func, const std::vector<Value *> &args)
            : Instruction(func->getType()->getFunctionRet(), CALL, merge(func, args)) {}

    std::string Instruction::call::to_string() const {
        std::stringstream ss;
        if (getFunction()->retType != Type::getVoidType())
            ss << getName() << " = ";
        ss << "call " << getFunction()->getType();
        ss << " " << getFunction()->getName() << "(";
        for (int i = 0; i < getNumArgs(); i++) {
            if (i) ss << ", ";
            ss << getArg(i);
        }
        ss << ")";
        return ss.str();
    }
}
