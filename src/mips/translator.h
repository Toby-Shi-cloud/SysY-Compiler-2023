//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_TRANSLATOR_H
#define COMPILER_MIPS_TRANSLATOR_H

#include <queue>
#include "backend/translator.h"
#include "mips/instruction.h"

namespace backend::mips {
class Translator : public TranslatorBase {
    std::queue<pAddress> usedAddress;

    template <typename... Args>
    auto newAddress(Args... args) -> decltype(new Address(std::forward<decltype(args)>(args)...)) {
        auto addr = new Address(std::forward<decltype(args)>(args)...);
        usedAddress.emplace(addr);
        return addr;
    }

    rAddress getAddress(const mir::Value *mirValue) {
        auto ptr = translateOperand(mirValue);
        if (auto addr = dynamic_cast<rAddress>(ptr)) return addr;
        if (auto reg = dynamic_cast<rRegister>(ptr)) return newAddress(reg, 0);
        if (auto label = dynamic_cast<rLabel>(ptr)) return newAddress(getZeroRegister(), 0, label);
        return nullptr;
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
    void translateBasicBlock(const mir::BasicBlock *mirBlock);
    void translateInstruction(const mir::Instruction *mirInst);

    void translateFunction(const mir::Function *mirFunction) override;
    void translateGlobalVar(const mir::GlobalVar *mirVar) override;
    rOperand translateOperand(const mir::Value *mirValue) override;
    static rRegister getZeroRegister() { return PhyRegister::get(0); }

    void compute_phi(const mir::Function *mirFunction);
    void compute_func_start() const;
    void compute_func_exit() const;
    void optimizeBeforeAlloc() const;
    void optimizeAfterAlloc() const;

 public:
    using TranslatorBase::TranslatorBase;
    void translate() override;
};
}  // namespace backend::mips

#endif  // COMPILER_MIPS_TRANSLATOR_H
