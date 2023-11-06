//
// Created by toby on 2023/11/6.
//

#ifndef COMPILER_MIPS_COMPONENT_H
#define COMPILER_MIPS_COMPONENT_H

#include <vector>
#include <ostream>
#include <unordered_set>
#include "operand.h"

namespace mips {
    struct Block {
        std::string name;
        rFunction parent;
        std::vector<pInstruction> instructions;
        std::vector<rBlock> predecessors, successors;
        std::vector<rRegister> liveIn, liveOut;
        std::vector<rRegister> use, def;
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
}

#endif //COMPILER_MIPS_COMPONENT_H
