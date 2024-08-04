//
// Created by toby on 2024/8/4.
//

#ifndef COMPILER_RISCV_TRANSLATOR_H
#define COMPILER_RISCV_TRANSLATOR_H

#include <memory>
#include <stack>
#include <utility>
#include "backend/translator.h"
#include "riscv/alias.h"
#include "riscv/instruction.h"

namespace backend::riscv {
class Translator : public TranslatorBase {
    // 在函数进行第一次 translate 的时候，将 sp 视作为函数进入前的值，因此需要再翻译。
    std::stack<int *> stack_imm_pointers;
    // 使用到的但是没有被其他指令持有的
    std::stack<pOperand> usedOperands;

    rAddress newAddress(rRegister base, pImmediate offset) {
        usedOperands.push(std::make_unique<Address>(base, std::move(offset)));
        return dynamic_cast<rAddress>(usedOperands.top().get());
    }

    rAddress getAddress(const mir::Value *mirValue) {
        auto ptr = translateOperand(mirValue);
        if (auto addr = dynamic_cast<rAddress>(ptr)) return addr;
        if (auto reg = dynamic_cast<rRegister>(ptr)) return newAddress(reg, 0_I);
        if (auto label = dynamic_cast<rLabel>(ptr)) return newAddress("x0"_R, create_imm(label));
        return nullptr;
    }

    template <Instruction::Ty rTy, Instruction::Ty iTy>
    rRegister createBinaryInstHelperX(rRegister lhs, mir::Value *rhs);
    template <mir::Instruction::InstrTy ty, size_t XLEN>
    rRegister translateBinaryInstHelper(rRegister lhs, mir::Value *rhs);

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

    void compute_phi(const mir::Function *mirFunction);
    void compute_func_start() const;
    void compute_func_exit() const;
    void optimizeBeforeAlloc() const;
    void optimizeAfterAlloc() const;

 public:
    using TranslatorBase::TranslatorBase;
};
}  // namespace backend::riscv

#endif  // COMPILER_RISCV_TRANSLATOR_H
