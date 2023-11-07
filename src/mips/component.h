//
// Created by toby on 2023/11/6.
//

#ifndef COMPILER_MIPS_COMPONENT_H
#define COMPILER_MIPS_COMPONENT_H

#include <vector>
#include <ostream>
#include <unordered_set>
#include "instruction.h"

namespace mips {
    struct Block {
        pLabel label;
        rFunction parent;
        std::vector<pInstruction> instructions;
        std::vector<rBlock> predecessors, successors;
        std::unordered_set<rRegister> liveIn, liveOut;
        std::unordered_set<rRegister> use, def;

        explicit Block(rFunction parent) : parent(parent) {}

        explicit Block(std::string name, rFunction parent)
                : label(std::make_unique<Label>(std::move(name))), parent(parent) {}
    };

    struct Function {
        pLabel label;
        unsigned allocaSize, argSize;
        pBlock startB = std::make_unique<Block>(this);
        pBlock exitB = std::make_unique<Block>("$BB." + label->name + ".end", this);
        std::vector<pBlock> blocks;
        std::unordered_set<pVirRegister> usedVirRegs;

        explicit Function(std::string name)
                : label(std::make_unique<Label>(std::move(name))),
                  allocaSize(0), argSize(0) {}

        explicit Function(std::string name, unsigned allocaSize, unsigned argSize)
                : label(std::make_unique<Label>(std::move(name))),
                  allocaSize(allocaSize), argSize(argSize) {}

        inline rVirRegister newVirRegister() {
            auto reg = new VirRegister();
            usedVirRegs.emplace(reg);
            return reg;
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
        if (block.label) os << block.label << ":" << std::endl;
        for (auto &inst: block.instructions)
            os << "\t" << *inst << std::endl;
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Function &func) {
        os << func.label << ":" << std::endl;
        os << *func.startB;
        for (auto &block: func.blocks)
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
