//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MIR_DERIVED_VALUE_H
#define COMPILER_MIR_DERIVED_VALUE_H

#include <set>
#include <list>
#include <memory>
#include <algorithm>
#include <unordered_set>
#include "value.h"

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

    using inst_node_t = std::list<Instruction *>::iterator;
    using inst_pos_t = std::list<Instruction *>::const_iterator;
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
        std::list<Instruction *> instructions;
        Function *parent;
        std::unordered_set<BasicBlock *> predecessors, successors;
        std::unordered_set<BasicBlock *> df; // dominator frontier
        std::set<BasicBlock *> dominators;
        BasicBlock *idom = nullptr;

        explicit BasicBlock(Function *parent) : Value(Type::getLabelType()), parent(parent) {}

        ~BasicBlock() override;

        void insert(inst_pos_t p, Instruction *inst);

        inst_node_t erase(const Instruction *inst);

        inst_node_t phi_end();

        inst_pos_t phi_end() const;

        void push_front(Instruction *inst) {
            insert(instructions.cbegin(), inst);
        }

        void push_back(Instruction *inst) {
            insert(instructions.cend(), inst);
        }

        void clear_info() {
            predecessors.clear();
            successors.clear();
            dominators.clear();
            df.clear();
            idom = {};
        }
    };

    /**
     * Function defs. <br>
     */
    struct Function : Value {
        pType retType;
        std::vector<Argument *> args; // owns
        std::list<BasicBlock *> bbs; // owns
        std::unique_ptr<BasicBlock> exitBB{new BasicBlock(this)};
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
        void allocName() const;

        /**
         * Clear all information of basic blocks. <br>
         */
        void clearBBInfo() const;

        /**
         * Calculate predecessors and successors for each basic block. <br>
         */
        void calcPreSuc() const;

        [[nodiscard]] bool isMain() const { return getName() == "@main"; }

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
        inst_node_t node{};

        virtual std::ostream &output(std::ostream &os) const = 0;

        [[nodiscard]] bool isTerminator() const { return instrTy == RET || instrTy == BR; }

        [[nodiscard]] bool isCall() const { return instrTy == CALL; }

        [[nodiscard]] bool isValue() const { return getType() != Type::getVoidType(); }

        template<typename... Args>
        explicit Instruction(pType type, InstrTy instrTy, Args... args) : User(type, args...), instrTy(instrTy) {}

        explicit Instruction(pType type, InstrTy instrTy, const std::vector<Value *> &args) :
            User(type, args), instrTy(instrTy) {}

        template<InstrTy ty>
        struct _binary_instruction;
        template<InstrTy ty>
        struct _conversion_instruction;

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

    struct BooleanLiteral : Literal {
        const bool value;
        static BooleanLiteral *trueLiteral;
        static BooleanLiteral *falseLiteral;

    private:
        explicit BooleanLiteral(bool value)
            : Literal(Type::getI1Type(), std::to_string(value)), value(value) {}
    };

    inline BooleanLiteral *BooleanLiteral::trueLiteral = new BooleanLiteral(true);
    inline BooleanLiteral *BooleanLiteral::falseLiteral = new BooleanLiteral(false);

    inline BooleanLiteral *getBooleanLiteral(bool value) {
        return value ? BooleanLiteral::trueLiteral : BooleanLiteral::falseLiteral;
    }

    struct IntegerLiteral : Literal {
        const int value;

        explicit IntegerLiteral(int value)
            : Literal(Type::getI32Type(), std::to_string(value)), value(value) {}

        explicit IntegerLiteral(unsigned value)
            : IntegerLiteral(static_cast<int>(value)) {}
    };

    IntegerLiteral *getIntegerLiteral(int value);

    inline IntegerLiteral *getIntegerLiteral(unsigned value) {
        return getIntegerLiteral(static_cast<int>(value));
    }

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

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, mir::BasicBlock *const &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << value->getName();
        return true;
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, mir::Function *const &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << value->getName();
        return true;
    }
}
#endif //DBG_ENABLE

#endif //COMPILER_MIR_DERIVED_VALUE_H
