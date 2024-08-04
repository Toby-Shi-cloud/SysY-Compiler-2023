//
// Created by toby on 2023/11/6.
//

#ifndef COMPILER_BACKEND_COMPONENT_H
#define COMPILER_BACKEND_COMPONENT_H

#include <list>
#include <ostream>
#include <unordered_set>
#include <vector>
#include "backend/operand.h"

namespace backend {
// InstructionBase
struct InstructionBase {
    std::vector<rRegister> regDef, regUse;
    std::unordered_set<rRegister> liveIn, liveOut;
    inst_node_t node{};  // the position where the inst is.
    rSubBlock parent{};

    virtual ~InstructionBase() noexcept {
        for (auto reg : regDef) reg->defUsers.erase(this);
        for (auto reg : regUse) reg->useUsers.erase(this);
    }

    InstructionBase() noexcept = default;
    InstructionBase(std::vector<rRegister> regDef, std::vector<rRegister> regUse) noexcept
        : regDef{std::move(regDef)}, regUse{std::move(regUse)} {
        for (auto reg : this->regDef) reg->defUsers.insert(this);
        for (auto reg : this->regUse) reg->useUsers.insert(this);
    }

    InstructionBase(const InstructionBase &inst) noexcept
        : InstructionBase(inst.regDef, inst.regUse) {}
    InstructionBase(InstructionBase &&inst) noexcept = delete;

    void reg_def_push_back(rRegister reg) {
        regDef.push_back(reg);
        reg->defUsers.insert(this);
    }

    void reg_use_push_back(rRegister reg) {
        regUse.push_back(reg);
        reg->useUsers.insert(this);
    }

    virtual bool isJumpBranch() const = 0;
    virtual bool isFuncCall() const = 0;
    virtual void setJumpLabel(rLabel newLabel) = 0;
    virtual rLabel getJumpLabel() const = 0;

    virtual std::ostream &output(std::ostream &os) const = 0;
    [[nodiscard]] virtual pInstructionBase clone() const = 0;

    template <typename T>
    [[nodiscard]] T clone_as() const {
        auto ptr = clone().release();
        auto inst = dynamic_cast<T>(ptr);
        if (inst == nullptr) delete ptr;
        return inst;
    }

    [[nodiscard]] rInstructionBase next() const;
};

inline std::ostream &operator<<(std::ostream &os, const InstructionBase &inst) {
    return inst.output(os);
}

inline std::ostream &operator<<(std::ostream &os, const pInstructionBase &inst) {
    return inst->output(os);
}

inline std::ostream &operator<<(std::ostream &os, rInstructionBase inst) {
    return inst->output(os);
}

/**
 * SubBlock is the unit to allocate registers. <br>
 * SubBlocks have no label. There is one and only one jump InstructionBase
 * (conditional jump to other block or unconditional jump to function)
 * at the end of every sub-block. Unconditional jump to other block or
 * function return jump is only allowed and must existed when then
 * sub-block is the last sub-block of all blocks. <br>
 * SubBlocks will be linked together by Blocks. One sub-block can only
 * be linked by one block. SubBlocks can also be cloned to support
 * multiply blocks (trade-off between code size and efficiency). <br>
 */
struct SubBlock {
    rBlock parent;
    std::list<pSubBlock>::iterator node;
    std::list<pInstructionBase> instructions;
    std::unordered_set<rSubBlock> predecessors, successors;
    std::unordered_set<rRegister> liveIn, liveOut;
    std::unordered_set<rRegister> use, def;

    explicit SubBlock(rBlock parent) : parent(parent) {}

    [[nodiscard]] bool empty() const { return instructions.empty(); }
    [[nodiscard]] auto back() const { return instructions.back().get(); }
    [[nodiscard]] auto begin() const { return instructions.begin(); }
    [[nodiscard]] auto end() const { return instructions.end(); }
    [[nodiscard]] auto begin() { return instructions.begin(); }
    [[nodiscard]] auto end() { return instructions.end(); }

    inst_node_t insert(inst_pos_t p, pInstructionBase &&inst) {
        auto self = inst.get();
        auto node_ = instructions.insert(p, std::move(inst));
        self->parent = this;
        return self->node = node_;
    }

    inst_node_t emplace(inst_pos_t p, rInstructionBase inst) {
        return insert(p, pInstructionBase{inst});
    }

    template <typename... Args>
    auto erase(Args... args)
        -> decltype(instructions.erase(std::forward<decltype(args)>(args)...)) {
        return instructions.erase(std::forward<decltype(args)>(args)...);
    }

    std::ostream &output(std::ostream &os, bool skip_last = false) const {
        for (auto &inst : instructions) {
            if (skip_last && &inst == &instructions.back()) break;
            os << "\t" << *inst << "\n";
        }
        return os;
    }

    void clearInfo() {
        predecessors.clear();
        successors.clear();
        liveIn.clear();
        liveOut.clear();
        use.clear();
        def.clear();
    }
};

struct Block {
    rFunction parent;
    block_node_t node{};
    std::list<pSubBlock> subBlocks;
    pLabel label{new Label("<unnamed block>", this)};

    explicit Block(rFunction parent) : parent(parent), subBlocks{} {
        subBlocks.emplace_back(new SubBlock(this));
        subBlocks.back()->node = std::prev(subBlocks.end());
    }

    [[nodiscard]] bool empty() const { return subBlocks.size() == 1 && frontBlock()->empty(); }
    [[nodiscard]] rSubBlock frontBlock() const { return subBlocks.front().get(); }
    [[nodiscard]] rSubBlock backBlock() const { return subBlocks.back().get(); }

    [[nodiscard]] rInstructionBase frontInst() const {
        return frontBlock()->instructions.front().get();
    }

    [[nodiscard]] rInstructionBase backInst() const {
        return backBlock()->instructions.back().get();
    }

    void push_back(pInstructionBase &&inst) {
        if (!empty() && backInst()->isJumpBranch() && !backInst()->isFuncCall()) {
            subBlocks.emplace_back(new SubBlock(this));
            subBlocks.back()->node = std::prev(subBlocks.end());
        }
        subBlocks.back()->insert(subBlocks.back()->end(), std::move(inst));
    }

    void push_back(std::pair<pInstructionBase, pInstructionBase> &&pair) {
        push_back(std::move(pair.first));
        push_back(std::move(pair.second));
    }

    rBlock splitBlock(inst_pos_t pos);
    [[nodiscard]] rLabel nextLabel() const;
    void clearBlockInfo() const;
    void computePreSuc() const;

    [[nodiscard]] rBlock getJumpTo() const {
        if (empty()) return nullptr;
        auto &&lbl = backInst()->getJumpLabel();
        if (lbl && std::holds_alternative<rBlock>(lbl->parent))
            return std::get<rBlock>(lbl->parent);
        return nullptr;
    }
};

struct Function {
    pLabel label;
    int stackOffset = 0;
    unsigned allocaSize, argSize;
    bool isMain, isLeaf, retValue;
    std::set<rRegister> shouldSave;
    std::list<pBlock> blocks;
    pBlock exitB = std::make_unique<Block>(this);
    std::unordered_set<pVirRegister> usedVirRegs;

    explicit Function(std::string name, bool isMain, bool isLeaf, bool retValue)
        : Function(std::move(name), 0, 0, isMain, isLeaf, retValue) {}

    explicit Function(std::string name, unsigned allocaSize, unsigned argSize, bool isMain,
                      bool isLeaf, bool retValue)
        : label(std::make_unique<Label>(std::move(name), this)),
          allocaSize(allocaSize),
          argSize(argSize),
          isMain(isMain),
          isLeaf(isLeaf),
          retValue(retValue) {}

    rVirRegister newVirRegister(bool is_float = false) {
        auto reg = new VirRegister(is_float);
        usedVirRegs.emplace(reg);
        return reg;
    }

    [[nodiscard]] auto begin() const { return blocks.begin(); }
    [[nodiscard]] auto begin() { return blocks.begin(); }
    [[nodiscard]] auto end() const { return blocks.end(); }
    [[nodiscard]] auto end() { return blocks.end(); }

    void calcBlockPreSuc() const {
        for (auto &block : *this) block->computePreSuc();
    }

    void allocName() const {
        size_t counter = 0;
        for (auto &bb : *this)
            bb->label->name = '.' + label->name + "_" + std::to_string(counter++);
        exitB->label->name = '.' + label->name + "_end";
        blocks.front()->label->name = "";
    }
};

struct GlobalVar {
    pLabel label;
    bool isInit, isString, isConst, inExtern{};
    int offsetofGp = 0;
    unsigned size;
    std::variant<std::string, std::vector<int>> elements;

    explicit GlobalVar(std::string name, bool isInit, bool isString, bool isConst, unsigned size,
                       std::variant<std::string, std::vector<int>> elements)
        : label(std::make_unique<Label>(std::move(name), this)),
          isInit(isInit),
          isString(isString),
          isConst(isConst),
          size(size),
          elements(std::move(elements)) {}
};

struct Module {
    pFunction main;
    std::vector<pFunction> functions;
    std::vector<pGlobalVar> globalVars;

    /**
     * Calculate the offset of global variables in .data section.
     * To make full use of $gp, try to place $gp at .data + 0x8000.
     */
    void calcGlobalVarOffset() const;
};

inline std::ostream &operator<<(std::ostream &os, const SubBlock &block) {
    return block.output(os);
}

inline std::ostream &operator<<(std::ostream &os, const Block &block) {
    if (!block.label->name.empty()) os << block.label << ":" << "\n";
    for (auto &sub : block.subBlocks)
        sub->output(os, &sub == &block.subBlocks.back() && block.nextLabel() &&
                            block.backInst()->getJumpLabel() == block.nextLabel());
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const Function &func) {
    func.allocName();
    os << func.label << ":" << "\n";
    for (auto &block : func) os << *block;
    os << *func.exitB;
    return os;
}
}  // namespace backend

#ifdef DBG_ENABLE
namespace dbg {
template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::rBlock &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << "(" << value->label << ")" << " ";
    return pretty_print(stream, value->subBlocks);
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::pBlock &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    stream << "(" << value->label << ")" << " ";
    return pretty_print(stream, value->subBlocks);
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::rFunction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    return pretty_print(stream, value->label);
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::pFunction &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    return pretty_print(stream, value->label);
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::rSubBlock &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    return pretty_print(stream, value->instructions);
}

template <>
[[maybe_unused]]
inline bool pretty_print(std::ostream &stream, const backend::pSubBlock &value) {
    if (value == nullptr) return pretty_print(stream, nullptr);
    return pretty_print(stream, value->instructions);
}
}  // namespace dbg
#endif  // DBG_ENABLE

#endif  // COMPILER_BACKEND_COMPONENT_H
