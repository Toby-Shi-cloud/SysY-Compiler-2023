//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_TRANSLATOR_H
#define COMPILER_TRANSLATOR_H

#include "../mir.h"
#include "register_alloca.h"

namespace backend {
    class Translator {
        mir::Manager *mirManager;
        mips::rModule mipsModule;
        std::unordered_map<mir::Function *, mips::rFunction> fMap;
        std::unordered_map<mir::BasicBlock *, mips::rBlock> bMap;
        std::unordered_map<mir::GlobalVar *, mips::rGlobalVar> gMap;
        std::unordered_map<mir::Value *, mips::rOperand> oMap;
        std::unordered_map<mips::rOperand, mir::Value *> rMap;

        void translateFunction(mir::Function *mirFunction);

        void translateBasicBlock(mir::BasicBlock *mirBlock);

        void translateInstruction(mir::Instruction *mirInst);

        void translateGlobalVar(mir::GlobalVar *mirVar);

        void translateOperand(mir::Value *mirValue);

        inline void translateFunctions() {
            for (auto func: mirManager->functions) {
                translateFunction(func);
            }
        }

        inline void translateGlobalVars() {
            for (auto var: mirManager->globalVars) {
                translateGlobalVar(var);
            }
        }

    public:
        explicit Translator(mir::Manager *mirManager, mips::rModule mipsModule)
                : mirManager(mirManager), mipsModule(mipsModule) {}

        inline void translate() {
            translateFunctions();
            translateGlobalVars();
        }
    };
}

#endif //COMPILER_TRANSLATOR_H
