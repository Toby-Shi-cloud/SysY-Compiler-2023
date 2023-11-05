//
// Created by toby on 2023/11/6.
//

#ifndef COMPILER_MIPS_COMPONENT_H
#define COMPILER_MIPS_COMPONENT_H

#include <vector>
#include <ostream>
#include <unordered_set>
#include "operand.h"
#include "instruction.h"

namespace mips {
    struct Block;
    struct Function;
    struct GlobalVar;
    struct Module;
}

namespace mips {
    struct Block {
        std::string name;
        Function *parent;
        std::vector<Instruction *> instructions;
        std::vector<Block *> predecessors, successors;
        std::vector<Register *> liveIn, liveOut;
        std::vector<Register *> use, def;
    };

    struct Function {
        std::string name;
        std::vector<Block *> blocks;
        std::unordered_set<VirRegister *> usedVirRegs;
        unsigned allocaSize, argSize;
    };

    struct GlobalVar {
        std::string name;
        bool isInit, isString, isConst;
        unsigned size;
        std::variant<std::string, std::vector<int>> elements;
    };

    struct Module {
        std::vector<Function *> functions;
        std::vector<GlobalVar *> globalVars;
        Function *main;
    };
}

#endif //COMPILER_MIPS_COMPONENT_H
