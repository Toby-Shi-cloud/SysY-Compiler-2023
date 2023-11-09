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
        rFunction parent;
        block_node_t node{};
        pLabel label = std::make_unique<Label>("");
        std::list<pInstruction> instructions;
        std::unordered_set<rBlock> predecessors, successors;
        std::unordered_set<rRegister> liveIn, liveOut;
        std::unordered_set<rRegister> use, def;
        pInstruction conditionalJump, fallthroughJump;

        explicit Block(rFunction parent) : parent(parent) {}

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

        [[nodiscard]] rLabel nextLabel() const;

        template<typename Self>
        struct inst_container_t {
            Self self;

            class block_iterator {
                using It = decltype(self->instructions.begin());
                Self self;
                It list_it;
                unsigned current;

            public:
                block_iterator(Self self, It list_it, unsigned current)
                        : self(self), list_it(list_it), current(current) {
                    if (self->instructions.empty()) ++*this;
                }

                inline block_iterator &operator++() {
                    if (list_it != self->instructions.end()) ++list_it;
                    if (list_it != self->instructions.end()) return *this;
                    if (current == 0) {
                        current = 1;
                        if (self->conditionalJump != nullptr) return *this;
                    }
                    if (current == 1) {
                        current = 2;
                        if (self->fallthroughJump != nullptr) return *this;
                    }
                    current = 3;
                    return *this;
                }

                inline block_iterator operator++(int) {
                    auto ret = *this;
                    ++*this;
                    return ret;
                }

                inline auto operator*() -> decltype(*list_it) {
                    if (list_it != self->instructions.end()) return *list_it;
                    if (current == 1) return self->conditionalJump;
                    if (current == 2) return self->fallthroughJump;
                    return *list_it;
                }

                inline auto operator->() -> decltype(list_it.operator->()) {
                    return &**this;
                }

                inline bool operator==(const block_iterator &it) const {
                    return self == it.self && list_it == it.list_it && current == it.current;
                }

                inline bool operator!=(const block_iterator &it) const {
                    return !(*this == it);
                }
            };

            [[nodiscard]] inline auto begin() const { return block_iterator{self, self->instructions.begin(), 0}; }

            [[nodiscard]] inline auto end() const { return block_iterator{self, self->instructions.end(), 3}; }

            [[nodiscard]] inline bool empty() const { return begin() == end(); }
        };

        [[nodiscard]] inst_container_t<Block *> allInstructions() { return {this}; }

        [[nodiscard]] inst_container_t<const Block *> allInstructions() const { return {this}; }

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
        std::list<pBlock> blocks;
        pBlock exitB = std::make_unique<Block>(this);
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
            for (auto it = begin(); it != end();) {
                auto &bb = *it;
                if (bb->allInstructions().empty()) it = blocks.erase(it);
                else bb->label->name = "$." + label->name + "_" + std::to_string(counter++), ++it;
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
        if (block.allInstructions().empty()) return os;
        if (!block.label->name.empty()) os << block.label << ":" << std::endl;
        for (auto &inst: block.instructions)
            os << "\t" << *inst << std::endl;
        if (block.conditionalJump) os << "\t" << *block.conditionalJump << std::endl;
        if (block.fallthroughJump && block.fallthroughJump->getJumpLabel() != block.nextLabel())
            os << "\t" << *block.fallthroughJump << std::endl;
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
        os << *module.main;
        for (auto &func: module.functions)
            os << std::endl << *func;
        return os;
    }
}

#endif //COMPILER_MIPS_COMPONENT_H
