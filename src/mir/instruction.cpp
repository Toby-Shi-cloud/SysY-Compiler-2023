//
// Created by toby on 2023/10/12.
//

#include "instruction.h"

namespace mir {
    static std::vector<Value *>
    merge(Value *ptr, std::vector<Value *>::const_iterator cbegin, std::vector<Value *>::const_iterator cend) {
        std::vector args = {ptr};
        args.insert(args.end(), cbegin, cend);
        return args;
    }

    template<typename T1, typename T2>
    static std::vector<Value *> flatten(const std::vector<std::pair<T1, T2>> &vec) {
        std::vector<Value *> ret;
        for (auto &&[x, y]: vec)
            ret.push_back(x), ret.push_back(y);
        return ret;
    }

    std::ostream &Instruction::ret::output(std::ostream &os) const {
        if (auto value = getReturnValue())
            return os << "ret " << value;

        return os << "ret void";
    }

    std::ostream &Instruction::br::output(std::ostream &os) const {
        if (hasCondition()) {
            assert(getCondition()->type == (pType) Type::getI1Type());
            return os << "br " << getCondition()
                   << ", label " << getIfTrue()->name
                   << ", label " << getIfFalse()->name;
        }
        return os << "br label " << getTarget()->name;
    }

    std::ostream &Instruction::fneg::output(std::ostream &os) const {
        return os << name << " = fneg " << getOperand();
    }

    std::ostream &Instruction::alloca_::output(std::ostream &os) const {
        return os << name << " = alloca " << type << ", align 4";
    }

    std::ostream &Instruction::load::output(std::ostream &os) const {
        return os << name << " = load " << type << ", ptr " << getPointerOperand()->name;
    }

    std::ostream &Instruction::store::output(std::ostream &os) const {
        return os << "store " << getSrc() << ", ptr " << getDest()->name;
    }

    Instruction::getelementptr::getelementptr(pType type, Value *ptr, const std::vector<Value *> &idxs)
        : Instruction(type, GETELEMENTPTR,
                      merge(ptr, idxs.begin() + ptr->type->isPointerTy(), idxs.end())),
          indexTy(getIndexTy(ptr->type)) {}

    std::ostream &Instruction::getelementptr::output(std::ostream &os) const {
        os << name << " = getelementptr " << indexTy
                << ", ptr " << getPointerOperand()->name;
        for (int i = 0; i < getNumIndices(); i++) {
            os << ", " << getIndexOperand(i);
        }
        return os;
    }

    std::ostream &Instruction::icmp::output(std::ostream &os) const {
        return os << name << " = icmp " << cond << " " << getLhs() << ", " << getRhs()->name;
    }

    std::ostream &Instruction::fcmp::output(std::ostream &os) const {
        return os << name << " = fcmp " << cond << " " << getLhs() << ", " << getRhs()->name;
    }

    Instruction::phi::phi(const std::vector<incominng_pair> &values)
        : Instruction(values[0].first->type, PHI, flatten(values)) {}

    std::ostream &Instruction::phi::output(std::ostream &os) const {
        os << name << " = phi " << type << " ";
        for (int i = 0; i < getNumIncomingValues(); i++) {
            if (i) os << ", ";
            auto [value, label] = getIncomingValue(i);
            os << "[ " << value->name << ", " << label->name << " ]";
        }
        return os;
    }

    Instruction::call::call(Function *func, const std::vector<Value *> &args)
        : Instruction(func->type->getFunctionRet(), CALL, merge(func, args.begin(), args.end())) {}

    std::ostream &Instruction::select::output(std::ostream &os) const {
        return os << name << " = select " << getCondition() << ", " << getTrueValue() << ", " << getFalseValue();
    }

    std::ostream &Instruction::call::output(std::ostream &os) const {
        if (getFunction()->retType != Type::getVoidType())
            os << name << " = ";
        os << "call " << getFunction()->type;
        os << " " << getFunction()->name << "(";
        for (int i = 0; i < getNumArgs(); i++) {
            if (i) os << ", ";
            os << getArg(i);
        }
        os << ")";
        return os;
    }

    std::ostream &Instruction::memset::output(std::ostream &os) const {
        return os << "call void @llvm.memset.p0.i32(ptr " << getBase()->name
                  << ", i8 " << val
                  << ", i32 " << size
                  << ", i1 false)";
    }
}
