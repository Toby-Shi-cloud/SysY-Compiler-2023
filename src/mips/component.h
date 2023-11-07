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
        std::string name;
        rFunction parent;
        std::vector<pInstruction> instructions;
        std::vector<rBlock> predecessors, successors;
        std::unordered_set<rRegister> liveIn, liveOut;
        std::unordered_set<rRegister> use, def;
    };

    struct Function {
        std::string name;
        std::vector<pBlock> blocks;
        std::unordered_set<pVirRegister> usedVirRegs;
        unsigned allocaSize, argSize;
    };

    struct GlobalVar {
        std::string name;
        bool isInit, isString, isConst;
        unsigned size;
        std::variant<std::string, std::vector<int>> elements;
    };

    struct Module {
        std::vector<pFunction> functions;
        std::vector<pGlobalVar> globalVars;
        pFunction main;
    };

    inline std::ostream &operator<<(std::ostream &os, const Block &block) {
        os << block.name << ":" << std::endl;
        for (auto &inst: block.instructions) {
            os << "  " << *inst << std::endl;
        }
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Function &func) {
        os << func.name << ":" << std::endl;
        for (auto &block: func.blocks) {
            os << *block;
        }
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
        os << var.name << ":" << std::endl;
        if (!var.isInit) os << "  .space " << var.size << std::endl;
        else if (var.isString) {
            os << "  .asciiz \"";
            for (char ch: std::get<std::string>(var.elements)) {
                if (ch == '\n') os << "\\n";
                else os << ch;
            }
            os << "\"" << std::endl;
        } else {
            auto &elements = std::get<std::vector<int>>(var.elements);
            for (auto ele: elements) os << "  .word " << ele << std::endl;
        }
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Module &module) {
        os << "  .data" << std::endl;
        for (auto &var: module.globalVars)
            if (!var->isString) os << *var;
        for (auto &var: module.globalVars)
            if (var->isString) os << *var;
        os << std::endl << "  .text" << std::endl;
        for (auto &func: module.functions)
            os << *func << std::endl;
        return os;
    }
}

#endif //COMPILER_MIPS_COMPONENT_H
