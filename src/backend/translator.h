//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_TRANSLATOR_H
#define COMPILER_TRANSLATOR_H

#include "mips.h"
#include "mir.h"

namespace backend {
class Translator {
    mir::Manager *mirManager;
    rModule mipsModule;
    std::unordered_map<const mir::Function *, rFunction> fMap;
    std::unordered_map<const mir::BasicBlock *, rBlock> bMap;
    std::unordered_map<const mir::GlobalVar *, rGlobalVar> gMap;
    std::unordered_map<const mir::Value *, rOperand> oMap;
    std::unordered_map<rOperand, const mir::Value *> rMap;
    rFunction curFunc = nullptr;
    rBlock curBlock = nullptr;

    void put(const mir::Value *value, rOperand operand) {
        if (oMap.count(value)) {
            auto old = dynamic_cast<rRegister>(oMap[value]);
            auto reg = dynamic_cast<rRegister>(operand);
            assert(old && reg);
            old->swapTo(reg);
            rMap.erase(old);
        }
        oMap[value] = operand;
        rMap[operand] = value;
    }

    template <mips::Instruction::Ty rTy, mips::Instruction::Ty iTy>
    rRegister createBinaryInstHelper(rRegister lhs, mir::Value *rhs);
    template <mir::Instruction::InstrTy ty>
    rRegister translateBinaryInstHelper(rRegister lhs, mir::Value *rhs);
    rRegister addressCompute(rAddress addr) const;
    void translateRetInst(const mir::Instruction::ret *retInst);
    void translateBranchInst(const mir::Instruction::br *brInst);
    template <mir::Instruction::InstrTy ty>
    void translateBinaryInst(const mir::Instruction::_binary_instruction<ty> *binInst);
    void translateAllocaInst(const mir::Instruction::alloca_ *allocaInst);
    void translateLoadInst(const mir::Instruction::load *loadInst);
    void translateStoreInst(const mir::Instruction::store *storeInst);
    void translateGetPtrInst(const mir::Instruction::getelementptr *getPtrInst);
    void translateConversionInst(const mir::Instruction::trunc *truncInst);
    void translateConversionInst(const mir::Instruction::zext *zextInst);
    void translateConversionInst(const mir::Instruction::sext *sextInst);
    void translateIcmpInst(const mir::Instruction::icmp *icmpInst);
    void translatePhiInst(const mir::Instruction::phi *phiInst);
    void translateSelectInst(const mir::Instruction::select *selectInst);
    void translateCallInst(const mir::Instruction::call *callInst);
    void translateFunction(const mir::Function *mirFunction);
    void translateBasicBlock(const mir::BasicBlock *mirBlock);
    void translateInstruction(const mir::Instruction *mirInst);
    void translateGlobalVar(const mir::GlobalVar *mirVar);
    rOperand translateOperand(const mir::Value *mirValue);

    rRegister getRegister(const mir::Value *mirValue) {
        return dynamic_cast<rRegister>(translateOperand(mirValue));
    }

    rAddress getAddress(const mir::Value *mirValue) {
        auto ptr = translateOperand(mirValue);
        if (auto addr = dynamic_cast<rAddress>(ptr)) return addr;
        if (auto reg = dynamic_cast<rRegister>(ptr)) return curFunc->newAddress(reg, 0);
        if (auto label = dynamic_cast<rLabel>(ptr))
            return curFunc->newAddress(mips::PhyRegister::get(0), 0, label);
        return nullptr;
    }

    void translateFunctions() {
        for (auto func : mirManager->functions) {
            translateFunction(func);
        }
    }

    void translateGlobalVars() {
        for (auto var : mirManager->globalVars) {
            translateGlobalVar(var);
        }
    }

    void compute_phi(const mir::Function *mirFunction);
    void compute_func_start() const;
    void compute_func_exit() const;
    void optimizeBeforeAlloc() const;
    void optimizeAfterAlloc() const;

 public:
    explicit Translator(mir::Manager *mirManager, rModule mipsModule)
        : mirManager(mirManager), mipsModule(mipsModule) {}

    void translate();
};
}  // namespace backend

#endif  // COMPILER_TRANSLATOR_H
