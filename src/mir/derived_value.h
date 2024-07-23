//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MIR_DERIVED_VALUE_H
#define COMPILER_MIR_DERIVED_VALUE_H

#include <algorithm>
#include <iomanip>
#include <list>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_set>
#include <variant>
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
using bb_node_t = std::list<BasicBlock *>::iterator;
using bb_pos_t = std::list<BasicBlock *>::const_iterator;

template <class T>
struct variant_helper {
    auto &as() const { return static_cast<const typename T::variant &>(*static_cast<const T *>(this)); }

    auto &as() { return static_cast<typename T::variant &>(*static_cast<T *>(this)); }
};

struct calculate_t : std::variant<int, float>, variant_helper<calculate_t> {
    using variant::variant;

    template <typename T>
    explicit operator T() const {
        return std::visit([](auto v) { return T(v); }, this->as());
    }

    calculate_t(unsigned value) : calculate_t(static_cast<int>(value)) {}  // NOLINT(google-explicit-constructor)
};

struct Interpreter {
    calculate_t retValue{};
    const BasicBlock *lastBB{};
    const BasicBlock *currentBB{};
    std::vector<calculate_t> stack{};
    std::unordered_map<const Value *, calculate_t> map{};
    std::unordered_map<const Value *, calculate_t> phi{};

    inline calculate_t getValue(const Value *value) const;
};
}  // namespace mir

namespace mir {
inline Literal *getZero(pType type);

/**
 * Argument is a function argument. <br>
 */
struct Argument : Value {
    Function *parent;

    explicit Argument(pType type, Function *parent) : Value(type), parent(parent) {}

    Argument(const Argument &) = delete;

    [[nodiscard]] Argument *clone(Function *_parent, value_map_t &map) const;
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
    bb_node_t node;
    std::unordered_set<BasicBlock *> predecessors, successors;
    std::unordered_set<BasicBlock *> df;  // dominator frontier
    std::set<BasicBlock *> dominators;
    BasicBlock *idom = nullptr;
    int loop_nest = 0;
    int dom_depth = 0;

    explicit BasicBlock(Function *parent) : Value(Type::getLabelType()), parent(parent) {}
    BasicBlock(const BasicBlock &) = delete;
    ~BasicBlock() override;
    void insert(inst_pos_t p, Instruction *inst);
    inst_node_t erase(const Instruction *inst);
    [[nodiscard]] inst_pos_t phi_end() const;
    [[nodiscard]] inst_pos_t beginner_end() const;
    void push_front(Instruction *inst) { insert(instructions.cbegin(), inst); }
    void push_back(Instruction *inst) { insert(instructions.cend(), inst); }
    void pop_front() { erase(*instructions.cbegin()); }
    void pop_back() { erase(*instructions.crbegin()); }
    void splice(inst_pos_t position, BasicBlock *other, inst_pos_t it);
    void splice(inst_pos_t position, BasicBlock *other, inst_pos_t first, inst_pos_t last);
    [[nodiscard]] BasicBlock *clone(Function *_parent, value_map_t &map) const;

    void clear_info() {
        predecessors.clear();
        successors.clear();
        dominators.clear();
        df.clear();
        idom = nullptr;
        loop_nest = 0;
        dom_depth = 0;
    }
};

/**
 * Function defs. <br>
 */
struct Function : Value {
    pType retType;
    bool isPure = false, noPostEffect = false;
    std::vector<Argument *> args;  // owns
    std::list<BasicBlock *> bbs;   // owns
    BasicBlock *exitBB = new BasicBlock(this);

    static Function *getint();
    static Function *getch();
    static Function *getfloat();
    static Function *getarray();
    static Function *getfarray();
    static Function *putint();
    static Function *putch();
    static Function *putfloat();
    static Function *putarray();
    static Function *putfarray();
    static Function *starttime();
    static Function *stoptime();

    static const auto &getLibrary() {
        const static std::array<Function *, 12> funcs = {
            getint(), getch(),    getfloat(), getarray(),  getfarray(), putint(),
            putch(),  putfloat(), putarray(), putfarray(), starttime(), stoptime(),
        };
        return funcs;
    }

    explicit Function(pType type, const std::string &name) : Value(type, "@" + name), retType(type->getFunctionRet()) {}

    Function(const Function &) = delete;

    ~Function() override;

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

    /**
     * Includes calcPreSuc, calcDominators and calcDF. <br>
     */
    void reCalcBBInfo() const {
        clearBBInfo();
        calcPreSuc();
        calcDominators();
        calcDF();
    }

    void calcDominators() const;
    void calcDF() const;
    void calcLoopNest() const;
    void calcDomDepth() const;

    [[nodiscard]] bool isMain() const { return name == "@main"; }
    [[nodiscard]] bool isLeaf() const;

    [[nodiscard]] bool isLibrary() const {
        return std::find(getLibrary().cbegin(), getLibrary().cend(), this) != getLibrary().cend();
    }

    [[nodiscard]] bool isRecursive() const;

    void markBBNode() {
        for (auto it = bbs.begin(); it != bbs.end(); ++it) (*it)->node = it;
        exitBB->node = bbs.end();
    }

    void splice(bb_pos_t position, Function *other, bb_pos_t first, bb_pos_t last) {
        for (auto it = first; it != last; ++it) (*it)->parent = this;
        bbs.splice(position, other->bbs, first, last);
    }

    [[nodiscard]] Function *clone() const;

    [[nodiscard]] calculate_t interpret(const std::vector<calculate_t> &_args_v) const;
};

/**
 * Global Variables / Constants. <br>
 */
struct GlobalVar : Value {
    Literal *init;  // nullptr if we should use ZeroInitializer
    const bool unnamed;
    const bool isConst;

    explicit GlobalVar(pType type, std::string name, Literal *init, bool isConst)
        : Value(type, "@" + std::move(name)), init(init), unnamed(false), isConst(isConst) {
        initialize();
    }

    explicit GlobalVar(pType type, Literal *init, bool isConst)
        : Value(type), init(init), unnamed(true), isConst(isConst) {
        initialize();
    }

    [[nodiscard]] bool isConstLVal() const override { return isConst; }

    GlobalVar(const GlobalVar &) = delete;

    ~GlobalVar() override;

 private:
    void initialize();
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
        FADD, FSUB, FMUL, FDIV, FREM,
        // Bitwise Binary Operations
        SHL, LSHR, ASHR, AND, OR, XOR,
        // Unary Operators
        FNEG,
        // Memory Access and Addressing Operations
        ALLOCA, LOAD, STORE, GETELEMENTPTR,
        // Conversion Operations
        TRUNC, ZEXT, SEXT, FPTOUI, FPTOSI, UITOFP, SITOFP,
        // Other Operations
        ICMP, FCMP, PHI, SELECT, CALL, MEMSET,
    } instrTy;

    BasicBlock *parent = nullptr;
    inst_node_t node{};

    virtual std::ostream &output(std::ostream &os) const = 0;

    [[nodiscard]] bool isTerminator() const { return instrTy == RET || instrTy == BR; }
    [[nodiscard]] bool isCall() const { return instrTy == CALL || instrTy == MEMSET; }
    [[nodiscard]] bool isPhy() const { return instrTy == PHI; }
    [[nodiscard]] bool isBeginner() const { return instrTy == PHI || instrTy == ALLOCA; }
    [[nodiscard]] bool isMemoryAccess() const { return instrTy == LOAD || instrTy == STORE || instrTy == MEMSET; }
    [[nodiscard]] bool isValue() const { return type != Type::getVoidType(); }

    template <typename... Args>
    explicit Instruction(pType type, InstrTy instrTy, Args... args) : User(type, args...), instrTy(instrTy) {}
    explicit Instruction(pType type, InstrTy instrTy, const std::vector<Value *> &args)
        : User(type, args), instrTy(instrTy) {}
    Instruction(const Instruction &inst) : User(inst), instrTy(inst.instrTy) {}
    Instruction(Instruction &&) = default;
    [[nodiscard]] virtual Instruction *clone() const = 0;
    virtual void interpret(Interpreter &interpreter) const = 0;

    template <InstrTy ty>
    struct _binary_instruction;
    template <InstrTy ty>
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
    using fadd = _binary_instruction<FADD>;
    using fsub = _binary_instruction<FSUB>;
    using fmul = _binary_instruction<FMUL>;
    using fdiv = _binary_instruction<FDIV>;
    using frem = _binary_instruction<FREM>;
    using shl = _binary_instruction<SHL>;
    using lshr = _binary_instruction<LSHR>;
    using ashr = _binary_instruction<ASHR>;
    using and_ = _binary_instruction<AND>;
    using or_ = _binary_instruction<OR>;
    using xor_ = _binary_instruction<XOR>;
    struct fneg;
    struct alloca_;
    struct load;
    struct store;
    struct getelementptr;
    using trunc = _conversion_instruction<TRUNC>;
    using zext = _conversion_instruction<ZEXT>;
    using sext = _conversion_instruction<SEXT>;
    using fptoui = _conversion_instruction<FPTOUI>;
    using fptosi = _conversion_instruction<FPTOSI>;
    using uitofp = _conversion_instruction<UITOFP>;
    using sitofp = _conversion_instruction<SITOFP>;
    struct icmp;
    struct fcmp;
    struct phi;
    struct select;
    struct call;
    struct memset;
};

/**
 * Literal is a constant value. <br>
 */
struct Literal : Value {
    explicit Literal(pType type) : Value(type) {}
    explicit Literal(pType type, std::string name) : Value(type, std::move(name)) {}
    [[nodiscard]] virtual calculate_t getValue() const { throw std::runtime_error("Cannot get " + name + "'s value"); }
};

struct BooleanLiteral : Literal {
    const bool value;
    static BooleanLiteral *trueLiteral;
    static BooleanLiteral *falseLiteral;
    [[nodiscard]] inline calculate_t getValue() const override { return value; }

 private:
    explicit BooleanLiteral(bool value) : Literal(Type::getI1Type(), std::to_string(value)), value(value) {}
};

inline BooleanLiteral *BooleanLiteral::trueLiteral = new BooleanLiteral(true);
inline BooleanLiteral *BooleanLiteral::falseLiteral = new BooleanLiteral(false);

inline BooleanLiteral *getBooleanLiteral(bool value) {
    return value ? BooleanLiteral::trueLiteral : BooleanLiteral::falseLiteral;
}

struct IntegerLiteral : Literal {
    const int value;
    explicit IntegerLiteral(int value) : Literal(Type::getI32Type(), std::to_string(value)), value(value) {}
    explicit IntegerLiteral(unsigned value) : IntegerLiteral(static_cast<int>(value)) {}
    [[nodiscard]] inline calculate_t getValue() const override { return value; }
};

IntegerLiteral *getIntegerLiteral(int value);

inline IntegerLiteral *getIntegerLiteral(unsigned value) { return getIntegerLiteral(static_cast<int>(value)); }

struct FloatLiteral : Literal {
    const float value;
    explicit FloatLiteral(float value) : Literal(Type::getFloatType(), stringify(value)), value(value) {}
    [[nodiscard]] inline calculate_t getValue() const override { return value; }
    [[nodiscard]] inline static std::string stringify(float v) {
        double d = v;
        std::stringstream ss;
        ss << std::scientific << v;
        if (std::stod(ss.str()) == d) return ss.str();
        std::stringstream ss2;
        ss2 << "0x" << std::setfill('0') << std::setw(16) << std::hex << std::uppercase
            << *reinterpret_cast<uint64_t *>(&d);
        return ss2.str();
    }
};

FloatLiteral *getFloatLiteral(float value);

inline Literal *getLiteral(int value) { return getIntegerLiteral(value); }
inline Literal *getLiteral(float value) { return getFloatLiteral(value); }

inline Literal *getLiteral(calculate_t value) {
    return std::visit([](auto v) { return getLiteral(v); }, value.as());
}

#define BIN_CALC_OP(op, ty)                                                                     \
    inline ty operator op(const calculate_t &lhs, const calculate_t &rhs) {                     \
        return std::visit([](auto v1, auto v2) -> ty { return v1 op v2; }, lhs.as(), rhs.as()); \
    }

#define BIN_LIT_OP_calculate_t(op)                                                                                     \
    inline Literal *operator op(const Literal & lhs, const Literal & rhs) {                                            \
        return std::visit([](auto v) -> Literal * { return getLiteral(v); }, (lhs.getValue() op rhs.getValue()).as()); \
    }

#define BIN_LIT_OP_bool(op)                                                 \
    inline Literal *operator op(const Literal & lhs, const Literal & rhs) { \
        return getBooleanLiteral(lhs.getValue() op rhs.getValue());         \
    }

#define BIN_OP(op, ty) BIN_CALC_OP(op, ty) BIN_LIT_OP_##ty(op)

BIN_OP(+, calculate_t)
BIN_OP(-, calculate_t)
BIN_OP(*, calculate_t)
BIN_OP(/, calculate_t)
BIN_OP(>, bool)
BIN_OP(>=, bool)
BIN_OP(<, bool)
BIN_OP(<=, bool)
BIN_OP(==, bool)
BIN_OP(!=, bool)

inline calculate_t operator%(const calculate_t &lhs, const calculate_t &rhs) {
    return std::visit(
        [](auto v1, auto v2) -> calculate_t {
            if constexpr (std::is_same_v<decltype(v1), float> || std::is_same_v<decltype(v2), float>) {
                using namespace std::literals::string_literals;
                throw std::runtime_error("Cannot apply operator % between "s + typeid(v1).name() + " and " +
                                         typeid(v2).name());
            } else {
                return {v1 % v2};
            }
        },
        lhs.as(), rhs.as());
}

BIN_LIT_OP_calculate_t(%)
#undef BIN_OP
#undef BIN_CALC_OP
#undef BIN_LIT_OP

struct StringLiteral : Literal {
    const std::string value;
    mutable size_t refCounter = 0;
    explicit StringLiteral(std::string value);
    [[nodiscard]] size_t *inlineRefCounter() const override { return &refCounter; }
};

struct ArrayLiteral : Literal {
    const std::vector<Literal *> values;
    mutable size_t refCounter = 0;
    explicit ArrayLiteral(std::vector<Literal *> values);
    ~ArrayLiteral() override;
    [[nodiscard]] size_t *inlineRefCounter() const override { return &refCounter; }
    [[nodiscard]] inline calculate_t getValue() const override { return values[0]->getValue(); }
};

struct ZeroInitializer : Literal {
    explicit ZeroInitializer(pType type) : Literal(type, "zeroinitializer") {}
    [[nodiscard]] inline calculate_t getValue() const override { return {0}; }
};

inline Literal *getZero(pType type) {
    if (type->isIntegerTy()) return getIntegerLiteral(0);
    if (type->isFloatTy()) return getFloatLiteral(0);
    assert(type->isArrayTy());
    static std::unordered_map<pType, ZeroInitializer *> cache;
    if (auto it = cache.find(type); it != cache.end()) return it->second;
    return cache[type] = new ZeroInitializer(type);
}

struct ArrayValue : Value {
    const std::vector<Value *> values;
    mutable size_t refCounter = 0;

    explicit ArrayValue(std::vector<Value *> values)
        : Value(ArrayType::getArrayType(values.size(), values.empty() ? Type::getI32Type() : values[0]->type)),
          values(std::move(values)) {
        for (auto v : this->values)
            if (auto counter = v->inlineRefCounter()) ++*counter;
    }

    ~ArrayValue() override;
    [[nodiscard]] size_t *inlineRefCounter() const override { return &refCounter; }
};

std::ostream &operator<<(std::ostream &os, const BasicBlock &bb);
std::ostream &operator<<(std::ostream &os, const Function &func);
std::ostream &operator<<(std::ostream &os, const GlobalVar &var);
std::ostream &operator<<(std::ostream &os, const Instruction &instr);
std::ostream &operator<<(std::ostream &os, const Literal &literal);
std::ostream &operator<<(std::ostream &os, const ArrayValue &array);

inline bool isZero(const Value *value) {
    return value == getIntegerLiteral(0) || value == getFloatLiteral(0) || dynamic_cast<const ZeroInitializer *>(value);
}

inline calculate_t Interpreter::getValue(const Value *value) const {
    if (auto lit = dynamic_cast<const Literal *>(value)) {
        try {
            return lit->getValue();
        } catch (const std::exception &e) {
        }
    }
    return map.at(value);
}
}  // namespace mir

#ifdef DBG_ENABLE
namespace dbg {
template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, mir::BasicBlock *const &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << value->name;
    return true;
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, mir::Function *const &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << value->name;
    return true;
}
}  // namespace dbg
#endif  // DBG_ENABLE

#endif  // COMPILER_MIR_DERIVED_VALUE_H
