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
    struct Block {
        using inst_node_t = std::list<pInstruction>::iterator;
        using inst_pos_t = std::list<pInstruction>::const_iterator;
        pLabel label;
        rFunction parent;
        std::list<pInstruction> instructions;
        std::unordered_set<rBlock> predecessors, successors;
        std::unordered_set<rRegister> liveIn, liveOut;
        std::unordered_set<rRegister> use, def;
        pInstruction conditionalJump, fallthroughJump;

        explicit Block(rFunction parent) : label(std::make_unique<Label>("")), parent(parent) {}

        explicit Block(std::string name, rFunction parent)
                : label(std::make_unique<Label>(std::move(name))), parent(parent) {}

        inline void push_back(pInstruction &&inst) {
            auto it = instructions.insert(instructions.cend(), std::move(inst));
            it->get()->node = it;
            it->get()->parent = this;
        }

        inline inst_node_t insert(inst_pos_t pos, pInstruction &&inst) {
            auto it = instructions.insert(pos, std::move(inst));
            it->get()->node = it;
            it->get()->parent = this;
            return it;
        }

        template<typename ...Args>
        inline auto erase(Args ...args) -> decltype(instructions.erase(args...)) {
            return instructions.erase(args...);
        }

        [[nodiscard]] inline auto begin() const { return instructions.begin(); }

        [[nodiscard]] inline auto begin() { return instructions.begin(); }

        [[nodiscard]] inline auto end() const { return instructions.end(); }

        [[nodiscard]] inline auto end() { return instructions.end(); }

        [[nodiscard]] inline bool empty() const { return instructions.empty() && !conditionalJump && !fallthroughJump; }

        /**
         * splice the block into two blocks.
         * @param pos: position of jal (function call inst)
         * @return the new block (nullptr if pos is the last instruction)
         */
        [[nodiscard]] rBlock splice(inst_node_t pos);

        /**
         * merge another block to the end. the block won't release after merge.
         * @param block the block to merge.
         */
        void merge(Block *block);
    };

    struct Function {
        pLabel label;
        unsigned allocaSize, argSize;
        const bool isMain, isLeaf;
        std::unordered_set<rPhyRegister> shouldSave;
        pBlock exitB = std::make_unique<Block>(this);
        std::list<pBlock> blocks;
        std::unordered_set<pVirRegister> usedVirRegs;
        std::unordered_set<pAddress> usedAddress;

        explicit Function(std::string name, bool isMain, bool isLeaf)
                : label(std::make_unique<Label>(std::move(name))),
                  allocaSize(0), argSize(0), isMain(isMain), isLeaf(isLeaf) {}

        explicit Function(std::string name, unsigned allocaSize, unsigned argSize, bool isMain, bool isLeaf)
                : label(std::make_unique<Label>(std::move(name))),
                  allocaSize(allocaSize), argSize(argSize),
                  isMain(isMain), isLeaf(isLeaf) {}

        inline rVirRegister newVirRegister() {
            auto reg = new VirRegister();
            usedVirRegs.emplace(reg);
            return reg;
        }

        template<typename ...Args>
        inline rAddress newAddress(Args ...args) {
            auto addr = new Address(args...);
            usedAddress.emplace(addr);
            return addr;
        }

        [[nodiscard]] inline auto begin() const { return blocks.begin(); }

        [[nodiscard]] inline auto begin() { return blocks.begin(); }

        [[nodiscard]] inline auto end() const { return blocks.end(); }

        [[nodiscard]] inline auto end() { return blocks.end(); }

        inline void allocName() {
            size_t counter = 0;
            for (auto &bb: blocks) {
                if (bb->empty()) continue;
                bb->label->name = "$." + label->name + "_" + std::to_string(counter++);
            }
            exitB->label->name = "$." + label->name + ".end";
            blocks.front()->label->name = "";
        }
    };

    struct GlobalVar {
        pLabel label;
        bool isInit, isString, isConst;
        unsigned size;
        std::variant<std::string, std::vector<int>> elements;

        explicit GlobalVar(std::string name, bool isInit, bool isString, bool isConst,
                           unsigned size, std::variant<std::string, std::vector<int>> elements)
                : label(std::make_unique<Label>(std::move(name))), isInit(isInit), isString(isString),
                  isConst(isConst), size(size), elements(std::move(elements)) {}
    };

    struct Module {
        pFunction main;
        std::vector<pFunction> functions;
        std::vector<pGlobalVar> globalVars;
    };

    inline std::ostream &operator<<(std::ostream &os, const Block &block) {
        if (block.empty()) return os;
        if (!block.label->name.empty()) os << block.label << ":" << std::endl;
        for (auto &inst: block)
            os << "\t" << *inst << std::endl;
        if (block.conditionalJump) os << "\t" << *block.conditionalJump << std::endl;
        if (block.fallthroughJump) os << "\t" << *block.fallthroughJump << std::endl;
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Function &func) {
        os << func.label << ":" << std::endl;
        for (auto &block: func)
            os << *block;
        os << *func.exitB;
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
        os << var.label << ":" << std::endl;
        if (!var.isInit) os << "\t.space\t" << var.size << std::endl;
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
        os << "\t.data" << std::endl;
        for (auto &var: module.globalVars)
            if (!var->isString) os << *var;
        for (auto &var: module.globalVars)
            if (var->isString) os << *var;
        os << std::endl << "\t.text" << std::endl;
        for (auto &func: module.functions)
            os << *func << std::endl;
        os << *module.main;
        return os;
    }
}

#endif //COMPILER_MIPS_COMPONENT_H
