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
        std::unordered_map<const mir::Function *, mips::rFunction> fMap;
        std::unordered_map<const mir::BasicBlock *, mips::rBlock> bMap;
        std::unordered_map<const mir::GlobalVar *, mips::rGlobalVar> gMap;
        std::unordered_map<const mir::Value *, mips::rOperand> oMap;
        std::unordered_map<mips::rOperand, const mir::Value *> rMap;
        mips::rFunction curFunc = nullptr;
        mips::rBlock curBlock = nullptr;

        void put(const mir::Value *value, mips::rOperand operand) {
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

        void translateRetInst(const mir::Instruction::ret *retInst);

        void translateBranchInst(const mir::Instruction::br *brInst);

        template<mir::Instruction::InstrTy ty>
        void translateBinaryInst(const mir::Instruction::_binary_instruction<ty> *binInst);

        void translateAllocaInst(const mir::Instruction::alloca_ *allocaInst);

        void translateLoadInst(const mir::Instruction::load *loadInst);

        void translateStoreInst(const mir::Instruction::store *storeInst);

        void translateGetPtrInst(const mir::Instruction::getelementptr *getPtrInst);

        template<mir::Instruction::InstrTy ty>
        void translateConversionInst(const mir::Instruction::_conversion_instruction<ty> *convInst);

        void translateIcmpInst(const mir::Instruction::icmp *icmpInst);

        void translatePhiInst(const mir::Instruction::phi *phiInst);

        void translateCallInst(const mir::Instruction::call *callInst);

        void translateFunction(const mir::Function *mirFunction);

        void translateBasicBlock(const mir::BasicBlock *mirBlock);

        void translateInstruction(const mir::Instruction *mirInst);

        void translateGlobalVar(const mir::GlobalVar *mirVar);

        mips::rOperand translateOperand(const mir::Value *mirValue);

        mips::rRegister getRegister(const mir::Value *mirValue) {
            return dynamic_cast<mips::rRegister>(translateOperand(mirValue));
        }

        mips::rAddress getAddress(const mir::Value *mirValue) {
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

        void compute_phi(const mir::Function *mirFunction);

        void compute_func_start() const;

        void compute_func_exit() const;

        void optimize() const;

        static void log(const mips::Function *func);

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
