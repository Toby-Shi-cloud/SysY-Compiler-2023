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

    GlobalVar::GlobalVar(pType type, std::string name, Literal *init, bool isConstant)
            : Value(type, isConstant), init(init), unnamed(false) {
        setName("@" + std::move(name));
        if (init) init->used++;
    }

    GlobalVar::GlobalVar(pType type, Literal *init, bool isConstant)
            : Value(type, isConstant), init(init), unnamed(true) {
        if (init) init->used++;
    }

    template<typename T>
    Literal::Literal(pType type, T value) : Value(type, true), value(value) {
        setName(value_string());
    }

    std::string Literal::value_string() const {
        std::stringstream ss;
        if (getType() == Type::getI1Type()) {
            ss << std::boolalpha << getBool();
        } else if (getType() == Type::getI32Type()) {
            ss << getInt();
        } else if (getType()->isStringTy()) {
            std::string str = getString();
            size_t pos;
            while ((pos = str.find('\n')) != std::string::npos) {
                str.replace(pos, 1, "\\0A");
            }
            ss << "c" << '"' << str << "\\00" << '"';
        } else if (getType()->isArrayTy()) {
            ss << '[';
            for (size_t i = 0; i < getArray().size(); i++) {
                if (i) ss << ", ";
                ss << getArray()[i];
            }
            ss << ']';
        } else {
            assert(false);
        }
        return ss.str();
    }

    inline namespace literal_operators {
        Literal make_literal(bool i1) {
            return Literal(Type::getI1Type(), i1);
        }

        Literal make_literal(int i32) {
            return Literal(Type::getI32Type(), i32);
        }

        Literal make_literal(const std::string &str) {
            return Literal(Type::getStringType((int) str.length() + 1), str);
        }

        Literal make_literal(std::vector<Literal *> array) {
            assert(!array.empty());
            for (auto item: array) item->used++;
            auto size = static_cast<int>(array.size());
            auto base = array[0]->getType();
            return Literal(ArrayType::getArrayType(size, base), std::move(array));
        }

        Literal operator+(const Literal &lhs, const Literal &rhs) {
            return make_literal(lhs.getInt() + rhs.getInt());
        }

        Literal operator-(const Literal &lhs, const Literal &rhs) {
            return make_literal(lhs.getInt() - rhs.getInt());
        }

        Literal operator*(const Literal &lhs, const Literal &rhs) {
            return make_literal(lhs.getInt() * rhs.getInt());
        }

        Literal operator/(const Literal &lhs, const Literal &rhs) {
            return make_literal(lhs.getInt() / rhs.getInt());
        }

        Literal operator%(const Literal &lhs, const Literal &rhs) {
            return make_literal(lhs.getInt() % rhs.getInt());
        }
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
        os << literal.getType() << " " << literal.value_string();
        return os;
    }

    std::ostream &operator<<(std::ostream &os, Instruction::InstrTy ty) {
        for (char c : magic_enum::enum_to_string(ty))
            os << static_cast<char>(::tolower(c));
        return os;
    }
}
