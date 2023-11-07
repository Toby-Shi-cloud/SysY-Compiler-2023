//
// Created by toby2 on 2023/10/12.
//

#include "instruction.h"

namespace mir {
    static inline std::vector<Value *>
    merge(Value *ptr, std::vector<Value *>::const_iterator cbegin, std::vector<Value *>::const_iterator cend) {
        std::vector<Value *> args = {ptr};
        args.insert(args.end(), cbegin, cend);
        return args;
    }

    std::ostream &Instruction::ret::output(std::ostream &os) const {
        if (auto value = getReturnValue()) {
            return os << "ret " << value;
        } else {
            return os << "ret void";
        }
    }

    std::ostream &Instruction::br::output(std::ostream &os) const {
        if (hasCondition()) {
            assert(getCondition()->getType() == (pType) Type::getI1Type());
            return os << "br " << getCondition() << ", label " << getIfTrue()->getName() << ", label " << getIfFalse()->getName();
        } else {
            return os << "br label " << getTarget()->getName();
        }
    }

    std::ostream &Instruction::alloca_::output(std::ostream &os) const {
        return os << getName() << " = alloca " << getType() << ", align 4";
    }

    std::ostream &Instruction::load::output(std::ostream &os) const {
        return os << getName() << " = load " << getType() << ", ptr " << getPointerOperand()->getName();
    }

    std::ostream &Instruction::store::output(std::ostream &os) const {
        return os << "store " << getSrc() << ", ptr " << getDest()->getName();
    }

    Instruction::getelementptr::getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs)
            : Instruction(type, GETELEMENTPTR, merge(ptr, idxs.begin() + ptr->getType()->isPointerTy(), idxs.end())) {}

    std::ostream &Instruction::getelementptr::output(std::ostream &os) const {
        os << getName() << " = getelementptr " << getIndexTy()
           << ", ptr " << getPointerOperand()->getName();
        for (int i = 0; i < getNumIndices(); i++) {
            os << ", " << getIndexOperand(i);
        }
        return os;
    }

    std::ostream &Instruction::icmp::output(std::ostream &os) const {
        return os << getName() << " = icmp " << cond << " " << getLhs() << ", " << getRhs()->getName();
    }

    std::ostream &Instruction::phi::output(std::ostream &os) const {
        os << getName() << " = phi " << getType() << " ";
        for (int i = 0; i < getNumIncomingValues(); i++) {
            if (i) os << ", ";
            auto [value, label] = getIncomingValue(i);
            os << "[ " << value->getName() << ", " << label->getName() << " ]";
        }
        return os;
    }

    Instruction::call::call(Function *func, const std::vector<Value *> &args)
            : Instruction(func->getType()->getFunctionRet(), CALL, merge(func, args.begin(), args.end())) {}

    std::ostream &Instruction::call::output(std::ostream &os) const {
        if (getFunction()->retType != Type::getVoidType())
            os << getName() << " = ";
        os << "call " << getFunction()->getType();
        os << " " << getFunction()->getName() << "(";
        for (int i = 0; i < getNumArgs(); i++) {
            if (i) os << ", ";
            os << getArg(i);
        }
        os << ")";
        return os;
    }
}
