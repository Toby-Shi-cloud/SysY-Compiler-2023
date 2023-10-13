//
// Created by toby on 2023/10/12.
//

#include "derived_value.h"

namespace mir {
    Function *Function::getint = new Function(FunctionType::getFunctionType(Type::getI32Type(), {}), "getint");
    Function *Function::putint = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {Type::getI32Type()}), "putint");
    Function *Function::putch = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {Type::getI32Type()}), "putch");
    Function *Function::putstr = new Function(
            FunctionType::getFunctionType(Type::getVoidType(), {PointerType::getPointerType(Type::getI8Type())}),
            "putstr");
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
                instruction->setName("%" + std::to_string(counter++));
            }
        }
    }

    inline namespace literal_operators {
        Literal make_literal(bool i1) {
            return Literal(Type::getI1Type(), i1);
        }

        Literal make_literal(int i32) {
            return Literal(Type::getI32Type(), i32);
        }

        Literal make_literal(const std::string &str) {
            return Literal(ArrayType::getArrayType((int)str.size() + 1, Type::getI8Type()), str);
        }

        Literal make_literal(const std::vector<Literal> &array) {
            assert(!array.empty());
            return Literal(ArrayType::getArrayType((int)array.size(), array[0].getType()), array);
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
        os << "define dso_local " << func.getType() << " " << func.getName() << "(";
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
        (var.init ? os << var.init : os << var.getType() << " zeroinitializer") << ", align ";
        os << (var.getType()->isStringTy() ? 1 : 4);
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const Instruction &instr) {
        return os << instr.to_string();
    }

    std::ostream &operator<<(std::ostream &os, const Literal &literal) {
        os << literal.getType() << " ";
        if (literal.getType() == Type::getI1Type()) {
            os << std::boolalpha << literal.getBool();
        } else if (literal.getType() == Type::getI32Type()) {
            os << literal.getInt();
        } else if (literal.getType()->isArrayTy()) {
            auto base = literal.getType()->getArrayBase();
            if (base == Type::getI8Type()) {
                os << '"' << literal.getString() << '"';
            } else {
                os << '[';
                for (size_t i = 0; i < literal.getArray().size(); i++) {
                    if (i) os << ", ";
                    os << literal.getArray()[i];
                }
                os << ']';
            }
        } else {
            assert(false);
        }
        return os;
    }

    std::ostream &operator<<(std::ostream &os, Instruction::InstrTy ty) {
        static constexpr const char *names[] = {
                // Terminator Instructions
                "ret", "br",
                // Binary Operations
                "add", "sub", "mul", "udiv", "sdiv", "urem", "srem",
                // Bitwise Binary Operations
                "shl", "lshr", "ashr", "and", "or", "xor",
                // Memory Access and Addressing Operations
                "alloca", "load", "store", "getelementptr",
                // Conversion Operations
                "trunc", "zext", "sext",
                // Other Operations
                "icmp", "phi", "call"
        };
        return os << names[ty];
    }
}
