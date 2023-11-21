//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_TRANSLATOR_H
#define COMPILER_TRANSLATOR_H

#include "../mir.h"
#include "../mips.h"

namespace backend {
    class Translator {
        int optimizeLevel;
        mir::Manager *mirManager;
        mips::rModule mipsModule;
        std::unordered_map<mir::Function *, mips::rFunction> fMap;
        std::unordered_map<mir::BasicBlock *, mips::rBlock> bMap;
        std::unordered_map<mir::GlobalVar *, mips::rGlobalVar> gMap;
        std::unordered_map<mir::Value *, mips::rOperand> oMap;
        std::unordered_map<mips::rOperand, mir::Value *> rMap;
        mips::rFunction curFunc = nullptr;
        mips::rBlock curBlock = nullptr;

        void put(mir::Value *value, mips::rOperand operand) {
            if (oMap.count(value)) {
                auto old = dynamic_cast<mips::rRegister>(oMap[value]);
                auto reg = dynamic_cast<mips::rRegister>(operand);
                assert(old && reg);
                old->swapTo(reg);
                rMap.erase(old);
            }
            oMap[value] = operand;
            rMap[operand] = value;
        }

        template<mips::Instruction::Ty rTy, mips::Instruction::Ty iTy>
        mips::rRegister createBinaryInstHelper(mips::rRegister lhs, mir::Value *rhs);

        template<mir::Instruction::InstrTy ty>
        mips::rRegister translateBinaryInstHelper(mips::rRegister lhs, mir::Value *rhs);

        mips::rRegister addressCompute(mips::rAddress addr) const;

        void translateRetInst(mir::Instruction::ret *retInst);

        void translateBranchInst(mir::Instruction::br *brInst);

        template<mir::Instruction::InstrTy ty>
        void translateBinaryInst(mir::Instruction::_binary_instruction<ty> *binInst);

        void translateAllocaInst(mir::Instruction::alloca_ *allocaInst);

        void translateLoadInst(mir::Instruction::load *loadInst);

        void translateStoreInst(mir::Instruction::store *storeInst);

        void translateGetPtrInst(mir::Instruction::getelementptr *getPtrInst);

        template<mir::Instruction::InstrTy ty>
        void translateConversionInst(mir::Instruction::_conversion_instruction<ty> *convInst);

        void translateIcmpInst(mir::Instruction::icmp *icmpInst);

        void translatePhiInst(mir::Instruction::phi *phiInst);

        void translateCallInst(mir::Instruction::call *callInst);

        void translateFunction(mir::Function *mirFunction);

        void translateBasicBlock(mir::BasicBlock *mirBlock);

        void translateInstruction(mir::Instruction *mirInst);

        void translateGlobalVar(mir::GlobalVar *mirVar);

        mips::rOperand translateOperand(mir::Value *mirValue);

        mips::rRegister getRegister(mir::Value *mirValue) {
            return dynamic_cast<mips::rRegister>(translateOperand(mirValue));
        }

        mips::rAddress getAddress(mir::Value *mirValue) {
            auto ptr = translateOperand(mirValue);
            if (auto addr = dynamic_cast<mips::rAddress>(ptr))
                return addr;
            if (auto reg = dynamic_cast<mips::rRegister>(ptr))
                return curFunc->newAddress(reg, 0);
            if (auto label = dynamic_cast<mips::rLabel>(ptr))
                return curFunc->newAddress(mips::PhyRegister::get(0), 0, label);
            return nullptr;
        }

        void translateFunctions() {
            for (auto func: mirManager->functions) {
                translateFunction(func);
            }
        }

        void translateGlobalVars() {
            for (auto var: mirManager->globalVars) {
                translateGlobalVar(var);
            }
        }

        void compute_phi(mir::Function *mirFunction);

        void compute_func_start() const;

        void compute_func_exit() const;

        void optimize() const;

    public:
        explicit Translator(mir::Manager *mirManager, mips::rModule mipsModule, int optimizeLevel = 2)
            : optimizeLevel(optimizeLevel), mirManager(mirManager), mipsModule(mipsModule) {}

        void translate() {
            translateGlobalVars();
            translateFunctions();
        }
    };
}

#endif //COMPILER_TRANSLATOR_H
