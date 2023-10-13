//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_DERIVED_VALUE_H
#define COMPILER_DERIVED_VALUE_H

#include "value.h"
#include <any>
#include <ostream>

/**
 * Derived Values <br>
 * Use struct instead of class to make all members public.
 */
namespace mir {
    struct Argument;
    struct BasicBlock;
    struct Function;
    struct GlobalVar;
    struct Instruction;
    struct Literal;
}

namespace mir {
    /**
     * Argument is a function argument. <br>
     */
    struct Argument : Value {
        Function *parent;

        explicit Argument(pType type, Function *parent) : Value(type), parent(parent) {}
    };

    /**
     * BasicBlock is a block which contains a list of instructions,
     * and always ends with a terminator instruction. <br>
     */
    struct BasicBlock : Value {
        /**
         * BasicBlock owns all instructions.
         */
        std::vector<Instruction *> instructions;

        explicit BasicBlock() : Value(Type::getLabelType()) {}

        ~BasicBlock() override;
    };

    /**
     * Function defs. <br>
     */
    struct Function : Value {
        pType retType;
        std::vector<Argument *> args; // owns
        std::vector<BasicBlock *> bbs; // owns
        static Function *getint;
        static Function *putint;
        static Function *putch;
        static Function *putstr;

        explicit Function(pType type, std::string name) : Value(type), retType(type->getFunctionRet()) {
            setName(std::move(name));
        }

        /**
         * Add an argument to the function. <br>
         */
        Argument *addArgument(pType type);

        /**
         * Alloc names for args, bbs and instructions.
         */
        void allocName();
    };

    /**
     * Global Variables / Constants. <br>
     */
    struct GlobalVar : Value {
        Literal *init; // nullptr if we should use ZeroInitializer
        const bool unnamed;

        explicit GlobalVar(pType type, std::string name, Literal *init, bool isConstant)
                : Value(type, isConstant), init(init), unnamed(false) {
            setName(std::move(name));
        }

        explicit GlobalVar(pType type, Literal *init, bool isConstant)
                : Value(type, isConstant), init(init), unnamed(true) {}
    };

    /**
     * Instructions. <br>
     */
    struct Instruction : User {
        enum InstrTy {
            // Terminator Instructions
            RET, BR,
            // Binary Operations
            ADD, SUB, MUL, UDIV, SDIV, UREM, SREM,
            // Bitwise Binary Operations
            SHL, LSHR, ASHR, AND, OR, XOR,
            // Memory Access and Addressing Operations
            ALLOCA, LOAD, STORE, GETELEMENTPTR,
            // Conversion Operations
            TRUNC, ZEXT, SEXT,
            // Other Operations
            ICMP, PHI, CALL
        } instrTy;

        [[nodiscard]] virtual std::string to_string() const = 0;

        template<typename... Args>
        explicit Instruction(pType type, InstrTy instrTy, Args ...args) : User(type, args...), instrTy(instrTy) {}

        explicit Instruction(pType type, InstrTy instrTy, const std::vector<Value *> &args) :
                User(type, args), instrTy(instrTy) {}

        template<InstrTy ty> struct _binary_instruction;
        template<InstrTy ty> struct _conversion_instruction;

        struct ret;
        struct br;
        using add = _binary_instruction<ADD>;
        using sub = _binary_instruction<SUB>;
        using mul = _binary_instruction<MUL>;
        using udiv = _binary_instruction<UDIV>;
        using sdiv = _binary_instruction<SDIV>;
        using urem = _binary_instruction<UREM>;
        using srem = _binary_instruction<SREM>;
        using shl = _binary_instruction<SHL>;
        using lshr = _binary_instruction<LSHR>;
        using ashr = _binary_instruction<ASHR>;
        using and_ = _binary_instruction<AND>;
        using or_ = _binary_instruction<OR>;
        using xor_ = _binary_instruction<XOR>;
        struct alloca_;
        struct load;
        struct store;
        struct getelementptr;
        using trunc = _conversion_instruction<TRUNC>;
        using zext = _conversion_instruction<ZEXT>;
        using sext = _conversion_instruction<SEXT>;
        struct icmp;
        struct phi;
        struct call;
    };

    /**
     * Literal is a constant value. <br>
     */
    struct Literal : Value {
        std::any value;

        template<typename T>
        explicit Literal(pType type, T value) : Value(type, true), value(value) {}

        [[nodiscard]] bool getBool() const { return *std::any_cast<bool>(&value); }

        [[nodiscard]] int getInt() const { return *std::any_cast<int>(&value); }

        [[nodiscard]] const std::string &getString() const { return *std::any_cast<std::string>(&value); }

        [[nodiscard]] const std::vector<Literal> &getArray() const { return *std::any_cast<std::vector<Literal>>(&value); }
    };

    inline namespace literal_operators {
        Literal make_literal(bool i1);

        Literal make_literal(int i32);

        Literal make_literal(const std::string &str);

        Literal make_literal(const std::vector<Literal> &array);

        Literal operator+(const Literal &lhs, const Literal &rhs);

        Literal operator-(const Literal &lhs, const Literal &rhs);

        Literal operator*(const Literal &lhs, const Literal &rhs);

        Literal operator/(const Literal &lhs, const Literal &rhs);

        Literal operator%(const Literal &lhs, const Literal &rhs);
    }

    std::ostream &operator<<(std::ostream &os, const BasicBlock &bb);

    std::ostream &operator<<(std::ostream &os, const Function &func);

    std::ostream &operator<<(std::ostream &os, const GlobalVar &var);

    std::ostream &operator<<(std::ostream &os, const Instruction &instr);

    std::ostream &operator<<(std::ostream &os, const Literal &literal);

    std::ostream &operator<<(std::ostream &os, Instruction::InstrTy ty);
}

#endif //COMPILER_DERIVED_VALUE_H
