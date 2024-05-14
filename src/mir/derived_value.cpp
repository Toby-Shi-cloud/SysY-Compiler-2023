//
// Created by toby on 2023/10/12.
//

#include <sstream>
#include <unordered_map>
#include "../enum.h"
#include "instruction.h"
#include "derived_value.h"

namespace mir {
    Function *Function::getint() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getI32Type(), {}), "getint");
        return const_cast<Function *>(&_f);
    }

    Function *Function::getch() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getI8Type(), {}), "getch");
        return const_cast<Function *>(&_f);
    }

    Function *Function::getfloat() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getFloatType(), {}), "getfloat");
        return const_cast<Function *>(&_f);
    }

    Function *Function::getarray() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getI32Type(), {Type::getPointerType(Type::getI32Type())}), "getarray");
        return const_cast<Function *>(&_f);
    }

    Function *Function::getfarray() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getI32Type(), {Type::getPointerType(Type::getFloatType())}), "getfarray");
        return const_cast<Function *>(&_f);
    }

    Function *Function::putint() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {Type::getI32Type()}), "putint");
        return const_cast<Function *>(&_f);
    }

    Function *Function::putch() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {Type::getI32Type()}), "putch");
        return const_cast<Function *>(&_f);
    }

    Function *Function::putfloat() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {Type::getFloatType()}), "putfloat");
        return const_cast<Function *>(&_f);
    }

    Function *Function::putarray() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {Type::getI32Type(), Type::getPointerType(Type::getI32Type())}), "putarray");
        return const_cast<Function *>(&_f);
    }

    Function *Function::putfarray() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {Type::getI32Type(), Type::getPointerType(Type::getFloatType())}), "putfarray");
        return const_cast<Function *>(&_f);
    }

    Function *Function::starttime() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {}), "starttime");
        return const_cast<Function *>(&_f);
    }

    Function *Function::stoptime() {
        const static Function _f(FunctionType::getFunctionType(
                Type::getVoidType(), {}), "stoptime");
        return const_cast<Function *>(&_f);
    }
}

namespace mir {
    Argument *Argument::clone(Function *_parent, value_map_t &map) const {
        auto arg = new Argument(getType(), _parent);
        map[this] = arg;
        return arg;
    }

    BasicBlock::~BasicBlock() {
        for (auto instruction: instructions)
            delete instruction;
    }

    void BasicBlock::insert(inst_pos_t p, Instruction *inst) {
        inst->node = instructions.insert(p, inst);
        inst->parent = this;
    }

    inst_node_t BasicBlock::erase(const Instruction *inst) {
        assert(this == inst->parent);
        assert(!inst->isUsed());
        auto &&ret = instructions.erase(inst->node);
        delete inst;
        return ret;
    }

    inst_pos_t BasicBlock::phi_end() const {
        return std::find_if_not(instructions.begin(), instructions.end(),
                                std::function<bool(Instruction *)>(&Instruction::isPhy));
    }

    inst_pos_t BasicBlock::beginner_end() const {
        return std::find_if_not(instructions.begin(), instructions.end(),
                                [](auto &&inst) { return inst->isBeginner(); });
    }

    void BasicBlock::splice(inst_pos_t position, BasicBlock *other, inst_pos_t it) {
        (*it)->parent = this;
        instructions.splice(position, other->instructions, it);
    }

    void BasicBlock::splice(inst_pos_t position, BasicBlock *other, inst_pos_t first, inst_pos_t last) {
        for (auto it = first; it != last; ++it)
            (*it)->parent = this;
        instructions.splice(position, other->instructions, first, last);
    }

    BasicBlock *BasicBlock::clone(Function *_parent, value_map_t &map) const {
        auto bb = new BasicBlock(_parent);
        map[this] = bb;
        for (auto &&inst: instructions) {
            auto newInst = inst->clone();
            bb->push_back(newInst);
            map[inst] = newInst;
        }
        return bb;
    }

    Function::~Function() {
        for (auto arg: args)
            delete arg;
        for (auto bb: bbs)
            delete bb;
        delete exitBB;
    }

    Argument *Function::addArgument(pType type) {
        auto arg = new Argument(type, this);
        args.push_back(arg);
        return arg;
    }

    void Function::allocName() const {
        size_t counter = 0;
        for (auto arg: args) {
            arg->setName("%" + std::to_string(counter++));
        }
        for (auto bb: bbs) {
            if (bb->instructions.empty()) continue;
            bb->setName("%" + std::to_string(counter++));
            for (auto instruction: bb->instructions) {
                if (!instruction->isValue()) continue;
                instruction->setName("%" + std::to_string(counter++));
            }
        }
    }

    bool Function::isLeaf() const {
        return std::all_of(bbs.begin(), bbs.end(), [](auto bb) {
            return std::all_of(bb->instructions.begin(), bb->instructions.end(), [](auto inst) {
                return inst->instrTy != Instruction::CALL;
            });
        });
    }

    bool Function::isRecursive() const {
        return std::any_of(bbs.begin(), bbs.end(), [this](auto bb) {
            return std::any_of(bb->instructions.begin(), bb->instructions.end(), [this](auto inst) {
                auto call = dynamic_cast<Instruction::call *>(inst);
                return call && call->getFunction() == this;
            });
        });
    }

    Function *Function::clone() const {
        value_map_t map;
        auto func = new Function(getType(), getName());
        for (auto &&arg: args)
            func->args.push_back(arg->clone(func, map));
        for (auto &&bb: bbs)
            func->bbs.push_back(bb->clone(func, map));
        for (auto &&bb: func->bbs)
            for (auto &&inst: bb->instructions)
                inst->substituteOperands(map);
        return func;
    }

    GlobalVar::~GlobalVar() {
        // IntegerLiteral is owned by pool.
        if (getType()->isNumberTy()) return;
        delete init;
    }

    IntegerLiteral *getIntegerLiteral(int value) {
        static std::unordered_map<int, IntegerLiteral *> integerPool;
        if (integerPool[value] == nullptr)
            integerPool[value] = new IntegerLiteral(value);
        return integerPool[value];
    }

    FloatLiteral *getFloatLiteral(float value) {
        static std::unordered_map<float, FloatLiteral *> floatPool;
        if (floatPool[value] == nullptr)
            floatPool[value] = new FloatLiteral(value);
        return floatPool[value];
    }

    static char hex(int x) {
        assert(0 <= x && x < 16);
        if (x < 10) return static_cast<char>('0' + x);
        return static_cast<char>('A' + x - 10);
    }

    StringLiteral::StringLiteral(std::string value)
        : Literal(Type::getStringType(value.length() + 1)), value(std::move(value)) {
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
        : Literal(ArrayType::getArrayType(
              (int) values.size(), values.empty() ? Type::getI32Type() : values[0]->getType())),
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

    std::ostream &operator<<(std::ostream &os, const BasicBlock &bb) {
        if (bb.parent->bbs.front() != &bb) {
            os << bb.getName().substr(1) << ":";
            for (auto i = bb.getName().length(); i < 50; i++) os << " ";
            os << "; preds = ";
            std::vector<BasicBlock *> predecessors{bb.predecessors.begin(), bb.predecessors.end()};
            std::sort(predecessors.begin(), predecessors.end(),
                      [](auto &&x, auto &&y) { return x->getId() < y->getId(); });
            bool first = true;
            for (auto pred: predecessors)
                os << (first ? "" : ", ") << pred->getName(), first = false;
            os << "\n";
        }
        for (auto instruction: bb.instructions) {
            os << "  " << instruction << "\n";
        }
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const Function &func) {
        assert(!func.isLibrary());
        func.allocName();
        func.calcPreSuc();
        os << "define dso_local " << func.retType << " " << func.getName() << "(";
        for (size_t i = 0; i < func.args.size(); i++) {
            if (i) os << ", ";
            os << func.args[i];
        }
        os << ") {\n";
        for (auto bb: func.bbs) {
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
        return os << magic_enum::enum_to_string_lower(ty);
    }

    ArrayValue::ArrayValue(std::vector<Value *> values)
        : Value(ArrayType::getArrayType(
                (int) values.size(), values.empty() ? Type::getI32Type() : values[0]->getType())),
          values(std::move(values)) {}
}
