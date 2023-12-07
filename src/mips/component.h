//
// Created by toby on 2023/11/6.
//

#ifndef COMPILER_MIPS_COMPONENT_H
#define COMPILER_MIPS_COMPONENT_H

#include <list>
#include <vector>
#include <ostream>
#include <unordered_set>
#include "instruction.h"

namespace mips {
    /**
     * SubBlock is the unit to allocate registers. <br>
     * SubBlocks have no label. There is one and only one jump instruction
     * (conditional jump to other block or unconditional jump to function)
     * at the end of every sub-block. Unconditional jump to other block or
     * function return jump is only allowed and must existed when then
     * sub-block is the last sub-block of all blocks. <br>
     * SubBlocks will be linked together by Blocks. One sub-block can only
     * be linked by one block. SubBlocks can also be cloned to support
     * multiply blocks (trade-off between code size and efficiency). <br>
     */
    struct SubBlock {
        std::list<pInstruction> instructions;
        std::unordered_set<rSubBlock> predecessors, successors;
        std::unordered_set<rRegister> liveIn, liveOut;
        std::unordered_set<rRegister> use, def;

        [[nodiscard]] bool empty() const { return instructions.empty(); }

        [[nodiscard]] auto back() const { return instructions.back().get(); }

        [[nodiscard]] auto begin() const { return instructions.begin(); }

        [[nodiscard]] auto end() const { return instructions.end(); }

        [[nodiscard]] auto begin() { return instructions.begin(); }

        [[nodiscard]] auto end() { return instructions.end(); }

        inst_node_t insert(inst_pos_t p, pInstruction &&inst) {
            auto self = inst.get();
            auto node = instructions.insert(p, std::move(inst));
            self->parent = this;
            return self->node = node;
        }

        inst_node_t emplace(inst_pos_t p, rInstruction inst) {
            return insert(p, pInstruction{inst});
        }

        template<typename... Args>
        auto erase(Args... args) -> decltype(instructions.erase(std::forward<decltype(args)>(args)...)) {
            return instructions.erase(std::forward<decltype(args)>(args)...);
        }

        // Only clone the instructions.
        [[nodiscard]] pSubBlock clone() const {
            auto subBlock = std::make_unique<SubBlock>();
            for (auto &inst: instructions)
                subBlock->insert(subBlock->end(), inst->clone());
            return subBlock;
        }

        std::ostream &output(std::ostream &os, bool sharp_last = false) const {
            for (auto &inst: instructions)
                os << "\t" << (sharp_last && &inst == &instructions.back() ? "# " : "")
                        << *inst << "\n";
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

        explicit Block(rFunction parent) : parent(parent) { subBlocks.emplace_back(new SubBlock()); }

        [[nodiscard]] bool empty() const { return subBlocks.size() == 1 && frontBlock()->empty(); }

        [[nodiscard]] rSubBlock frontBlock() const { return subBlocks.front().get(); }

        [[nodiscard]] rSubBlock backBlock() const { return subBlocks.back().get(); }

        [[nodiscard]] rInstruction frontInst() const { return frontBlock()->instructions.front().get(); }

        [[nodiscard]] rInstruction backInst() const { return backBlock()->instructions.back().get(); }

        void push_back(pInstruction &&inst) {
            if (!empty() && backInst()->isJumpBranch() && !backInst()->isFuncCall())
                subBlocks.emplace_back(new SubBlock());
            subBlocks.back()->insert(subBlocks.back()->end(), std::move(inst));
        }

        void push_back(std::pair<pInstruction, pInstruction> &&pair) {
            push_back(std::move(pair.first));
            push_back(std::move(pair.second));
        }

        [[nodiscard]] rLabel nextLabel() const;

        void clearBlockInfo() const;

        void computePreSuc() const;

        // Only clone the instructions.
        [[nodiscard]] pBlock clone() const {
            auto block = std::make_unique<Block>(parent);
            block->subBlocks.clear();
            for (auto &sub: subBlocks)
                block->subBlocks.push_back(sub->clone());
            return block;
        }

        void mergeBlock(rBlock other) {
            assert(std::get<rBlock>(backInst()->getJumpLabel()->parent) == other);
            backBlock()->instructions.pop_back();
            if (backBlock()->empty()) subBlocks.pop_back();
            subBlocks.splice(subBlocks.end(), other->clone()->subBlocks);
            // other free here...
        }
    };

    struct Function {
        pLabel label;
        int stackOffset = 0;
        unsigned allocaSize, argSize;
        const bool isMain, isLeaf;
        PhyRegister::phy_set_t shouldSave;
        std::list<pBlock> blocks;
        pBlock exitB = std::make_unique<Block>(this);
        std::unordered_set<pVirRegister> usedVirRegs;
        std::unordered_set<pAddress> usedAddress;

        explicit Function(std::string name, bool isMain, bool isLeaf)
            : Function(std::move(name), 0, 0, isMain, isLeaf) {}

        explicit Function(std::string name, unsigned allocaSize, unsigned argSize, bool isMain, bool isLeaf)
            : label(std::make_unique<Label>(std::move(name), this)),
              allocaSize(allocaSize), argSize(argSize),
              isMain(isMain), isLeaf(isLeaf) {}

        rVirRegister newVirRegister() {
            auto reg = new VirRegister();
            usedVirRegs.emplace(reg);
            return reg;
        }

        template<typename... Args>
        auto newAddress(Args... args) -> decltype(new Address(std::forward<decltype(args)>(args)...)) {
            auto addr = new Address(std::forward<decltype(args)>(args)...);
            usedAddress.emplace(addr);
            return addr;
        }

        [[nodiscard]] auto begin() const { return blocks.begin(); }

        [[nodiscard]] auto begin() { return blocks.begin(); }

        [[nodiscard]] auto end() const { return blocks.end(); }

        [[nodiscard]] auto end() { return blocks.end(); }

        void calcBlockPreSuc() const {
            for (auto &block: *this)
                block->computePreSuc();
        }

        void allocName() const {
            size_t counter = 0;
            for (auto &bb: *this)
                bb->label->name = "$." + label->name + "_" + std::to_string(counter++);
            exitB->label->name = "$." + label->name + ".end";
            blocks.front()->label->name = "";
        }
    };

    struct GlobalVar {
        pLabel label;
        bool isInit, isString, isConst, inExtern{};
        int offsetofGp = 0;
        unsigned size;
        std::variant<std::string, std::vector<int>> elements;

        explicit GlobalVar(std::string name, bool isInit, bool isString, bool isConst,
                           unsigned size, std::variant<std::string, std::vector<int>> elements)
            : label(std::make_unique<Label>(std::move(name), this)), isInit(isInit), isString(isString),
              isConst(isConst), size(size), elements(std::move(elements)) {}
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
        for (auto &sub: block.subBlocks)
            sub->output(os, &sub == &block.subBlocks.back() && block.nextLabel()
                            && block.backInst()->getJumpLabel() == block.nextLabel());
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Function &func) {
        func.allocName();
        os << func.label << ":" << "\n";
        for (auto &block: func)
            os << *block;
        if (!func.exitB->empty()) os << *func.exitB;
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
        if (var.inExtern) return os << "\t.extern\t" << var.label << ", " << var.size << "\n";
        os << var.label << ":" << "\n";
        if (!var.isInit) os << "\t.space\t" << var.size << "\n";
        else if (var.isString) {
            os << "\t.asciiz\t\"";
            for (char ch: std::get<std::string>(var.elements)) {
                if (ch == '\n') os << "\\n";
                else os << ch;
            }
            os << "\"" << std::endl;
        } else {
            auto &elements = std::get<std::vector<int>>(var.elements);
            os << "\t.word" << "\t";
            bool first = true;
            for (auto ele: elements) os << (first ? "" : ", ") << ele, first = false;
            os << std::endl;
        }
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Module &module) {
        os << "\t.data" << "\n";
        for (auto &var: module.globalVars)
            if (!var->isString) os << *var;
        for (auto &var: module.globalVars)
            if (var->isString) os << *var;
        os << "\n" << "\t.text" << "\n";
        os << *module.main;
        for (auto &func: module.functions)
            os << std::endl << *func;
        return os;
    }
}

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::rBlock &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << "(" << value->label << ")" << " ";
        return pretty_print(stream, value->subBlocks);
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::pBlock &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        stream << "(" << value->label << ")" << " ";
        return pretty_print(stream, value->subBlocks);
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::rFunction &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        return pretty_print(stream, value->label);
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::pFunction &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        return pretty_print(stream, value->label);
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::rSubBlock &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        return pretty_print(stream, value->instructions);
    }

    template<>
    [[maybe_unused]]
    inline bool pretty_print(std::ostream &stream, const mips::pSubBlock &value) {
        if (value == nullptr) return pretty_print(stream, nullptr);
        return pretty_print(stream, value->instructions);
    }
}
#endif //DBG_ENABLE

#endif //COMPILER_MIPS_COMPONENT_H
