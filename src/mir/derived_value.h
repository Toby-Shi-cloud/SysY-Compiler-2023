//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MIR_DERIVED_VALUE_H
#define COMPILER_MIR_DERIVED_VALUE_H

#include <any>
#include <ostream>
#include <algorithm>
#include "value.h"
#include "../enum.h"

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

        void push_back(Instruction *instr);
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

        explicit Function(pType type, const std::string &name) : Value(type), retType(type->getFunctionRet()) {
            setName("@" + name);
        }

        /**
         * Add an argument to the function. <br>
         */
        Argument *addArgument(pType type);

        /**
         * Alloc names for args, bbs and instructions.
         */
        void allocName();

        [[nodiscard]] inline bool isMain() const { return getName() == "@main"; }

        [[nodiscard]] bool isLeaf() const;
    };

    /**
     * Global Variables / Constants. <br>
     */
    struct GlobalVar : Value {
        Literal *init; // nullptr if we should use ZeroInitializer
        const bool unnamed;

        explicit GlobalVar(pType type, std::string name, Literal *init, bool isConstant)
                : Value(type, isConstant), init(init), unnamed(false) {
            setName("@" + std::move(name));
        }

        explicit GlobalVar(pType type, Literal *init, bool isConstant)
                : Value(type, isConstant), init(init), unnamed(true) {}

        ~GlobalVar() override;
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
        BasicBlock *parent = nullptr;

        virtual std::ostream &output(std::ostream &os) const = 0;

        [[nodiscard]] inline bool isTerminator() const { return instrTy == RET || instrTy == BR; }

        [[nodiscard]] inline bool isValue() const { return getType() != Type::getVoidType(); }

        [[nodiscard]] inline bool activeAfter() const {
            return std::any_of(use->users.begin(), use->users.end(), [this](auto user) {
                return dynamic_cast<mir::Instruction *>(user)->parent != parent;
            });
        }

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
        explicit Literal(pType type) : Value(type, true) {}

        explicit Literal(pType type, std::string name) : Value(type, true) {
            setName(std::move(name));
        }
    };

    struct IntegerLiteral : Literal {
        const int value;

        explicit IntegerLiteral(int value)
                : Literal(Type::getI32Type(), std::to_string(value)), value(value) {}
    };

    IntegerLiteral operator+(const IntegerLiteral &lhs, const IntegerLiteral &rhs);

    IntegerLiteral operator-(const IntegerLiteral &lhs, const IntegerLiteral &rhs);

    IntegerLiteral operator*(const IntegerLiteral &lhs, const IntegerLiteral &rhs);

    IntegerLiteral operator/(const IntegerLiteral &lhs, const IntegerLiteral &rhs);

    IntegerLiteral operator%(const IntegerLiteral &lhs, const IntegerLiteral &rhs);

    struct StringLiteral : Literal {
        const std::string value;

        explicit StringLiteral(std::string value);
    };

    struct ArrayLiteral : Literal {
        const std::vector<Literal *> values;

        explicit ArrayLiteral(std::vector<Literal *> values);

        ~ArrayLiteral() override;
    };

    std::ostream &operator<<(std::ostream &os, const BasicBlock &bb);

    std::ostream &operator<<(std::ostream &os, const Function &func);

    std::ostream &operator<<(std::ostream &os, const GlobalVar &var);

    std::ostream &operator<<(std::ostream &os, const Instruction &instr);

    std::ostream &operator<<(std::ostream &os, const Literal &literal);

    std::ostream &operator<<(std::ostream &os, Instruction::InstrTy ty);
}

#endif //COMPILER_MIR_DERIVED_VALUE_H
