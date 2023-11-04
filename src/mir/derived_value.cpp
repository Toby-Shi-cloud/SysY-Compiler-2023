//
// Created by toby on 2023/10/12.
//

#include "derived_value.h"
#include <sstream>

namespace mir {
    Function *Function::getint = new Function(
            FunctionType::getFunctionType(Type::getI32Type(), {}), "getint");
    Function *Function::putint = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {Type::getI32Type()}), "putint");
    Function *Function::putch = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {Type::getI32Type()}), "putch");
    Function *Function::putstr = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {Type::getStringType()}), "putstr");
}

namespace mir {
    BasicBlock::~BasicBlock() {
        for (auto instruction: instructions)
            delete instruction;
    }

    Argument *Function::addArgument(pType type) {
        auto arg = new Argument(type, this);
        args.push_back(arg);
        return arg;
    }

    void Function::allocName() {
        size_t counter = 0;
        for (auto arg : args) {
            arg->setName("%" + std::to_string(counter++));
        }
        for (auto bb : bbs) {
            if (bb->instructions.empty()) continue;
            bb->setName("%" + std::to_string(counter++));
            for (auto instruction : bb->instructions) {
                if (!instruction->isValue()) continue;
                instruction->setName("%" + std::to_string(counter++));
            }
        }
    }

    GlobalVar::~GlobalVar() {
        // IntegerLiteral is owned by pool.
        if (getType()->isIntegerTy()) return;
        delete init;
    }

    IntegerLiteral operator+(const IntegerLiteral &lhs, const IntegerLiteral &rhs) {
        return IntegerLiteral(lhs.value + rhs.value);
    }

    IntegerLiteral operator-(const IntegerLiteral &lhs, const IntegerLiteral &rhs) {
        return IntegerLiteral(lhs.value - rhs.value);
    }

    IntegerLiteral operator*(const IntegerLiteral &lhs, const IntegerLiteral &rhs) {
        return IntegerLiteral(lhs.value * rhs.value);
    }

    IntegerLiteral operator/(const IntegerLiteral &lhs, const IntegerLiteral &rhs) {
        return IntegerLiteral(lhs.value / rhs.value);
    }

    IntegerLiteral operator%(const IntegerLiteral &lhs, const IntegerLiteral &rhs) {
        return IntegerLiteral(lhs.value % rhs.value);
    }

    static inline char hex(int x) {
        assert(0 <= x && x < 16);
        if (x < 10) return static_cast<char>('0' + x);
        else return static_cast<char>('A' + x - 10);
    }

    StringLiteral::StringLiteral(std::string value)
            : Literal(Type::getStringType((int) value.length() + 1)), value(std::move(value)) {
        std::string s = R"(c")";
        for (char c: this->value) {
            if (c < 0x20) {
                s += '\\';
                s += hex(c / 16);
                s += hex(c % 16);
            } else if (c == '\\') {
                s += R"(\\)";
            } else if (c == '"') {
                s += R"(\")";
            } else {
                s += c;
            }
        }
        s += R"(\00")";
        setName(std::move(s));
    }

    ArrayLiteral::ArrayLiteral(std::vector<Literal *> values)
            : Literal(ArrayType::getArrayType((int) values.size(), values[0]->getType())),
              values(std::move(values)) {
        std::string s = "[";
        for (size_t i = 0; i < this->values.size(); i++) {
            if (i) s += ", ";
            s += this->values[i]->getType()->to_string();
            s += " ";
            s += this->values[i]->getName();
        }
        s += ']';
        setName(std::move(s));
    }

    ArrayLiteral::~ArrayLiteral() {
        // IntegerLiteral is owned by pool.
        if (getType()->getArrayBase()->isIntegerTy()) return;
        for (auto value: values)
            delete value;
    }

    std::ostream &operator<<(std::ostream &os, const BasicBlock &bb) {
        os << bb.getName().substr(1) << ":\n";
        for (auto instruction : bb.instructions) {
            os << "  " << instruction << "\n";
        }
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const Function &func) {
        os << "define dso_local " << func.retType << " " << func.getName() << "(";
        for (size_t i = 0; i < func.args.size(); i++) {
            if (i) os << ", ";
            os << func.args[i];
        }
        os << ") {\n";
        for (auto bb : func.bbs) {
            os << bb;
        }
        os << "}\n";
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
        os << var.getName() << " = ";
        os << (var.unnamed ? "private unnamed_addr " : "dso_local ");
        os << (var.isConst() ? "constant " : "global ");
        if (var.init) os << var.init;
        else os << var.getType() << (var.getType()->isIntegerTy() ? " 0" : " zeroinitializer");
        os << ", align " << (var.getType()->isStringTy() ? 1 : 4);
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const Instruction &instr) {
        return instr.output(os);
    }

    std::ostream &operator<<(std::ostream &os, const Literal &literal) {
        os << literal.getType() << " " << literal.getName();
        return os;
    }

    std::ostream &operator<<(std::ostream &os, Instruction::InstrTy ty) {
        for (char c : magic_enum::enum_to_string(ty))
            os << static_cast<char>(::tolower(c));
        return os;
    }
}
