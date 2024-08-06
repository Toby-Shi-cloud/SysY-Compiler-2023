//
// Created by toby on 2024/8/4.
//

#ifndef COMPILER_RISCV_TRANSLATOR_H
#define COMPILER_RISCV_TRANSLATOR_H

#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include "backend/translator.h"
#include "riscv/alias.h"
#include "riscv/instruction.h"

namespace backend::riscv {
inline std::list<pInstruction> translateImmAs(rRegister reg, int imm) {
    std::list<pInstruction> instructions;
    if (imm == 0) {
        instructions.push_back(std::make_unique<MoveInstruction>(reg, "x0"_R));
        return instructions;
    }
    auto u = static_cast<unsigned>(imm);
    auto hi = u >> 12, lo = u & 0xFFF;
    if (lo & 0x800) hi = (hi + 1) & 0xFFFFF, lo |= 0xFFFF'F000;
    if (hi) {
        instructions.push_back(
            std::make_unique<UInstruction>(Instruction::Ty::LUI, reg, create_imm((int)hi)));
    }
    if ((imm & 0x0fff) != 0) {
        instructions.push_back(std::make_unique<IInstruction>(
            Instruction::Ty::ADDIW, reg, hi ? (rRegister)reg : (rRegister) "x0"_R,
            create_imm((int)lo)));
    }
    return instructions;
}

class Translator : public TranslatorBase {
    // 使用到的但是没有被其他指令持有的
    std::stack<pOperand> used_operands;
    // 库函数 Label
    std::unordered_map<std::string, pLabel> lib_labels;

    rAddress newAddress(rRegister base, pImmediate offset) {
        used_operands.push(std::make_unique<Address>(base, std::move(offset)));
        return dynamic_cast<rAddress>(used_operands.top().get());
    }

    rAddress getAddress(const mir::Value *mirValue) {
        auto ptr = translateOperand(mirValue);
        if (auto addr = dynamic_cast<rAddress>(ptr)) return addr;
        if (auto reg = dynamic_cast<rRegister>(ptr)) return newAddress(reg, 0_I);
        if (auto label = dynamic_cast<rLabel>(ptr)) return newAddress("x0"_R, create_imm(label));
        return nullptr;
    }

    rRegister addr2reg(rAddress addr) {
        auto temp = curFunc->newVirRegister();
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::ADDI, temp, addr->base,
                                                           addr->offset->clone()));
        return temp;
    }

    rLabel getLibLabel(const std::string &name) {
        auto &label = lib_labels[name];
        if (label == nullptr) label = std::make_unique<Label>(name.substr(1));
        return label.get();
    }

    rRegister getRegister(const mir::Value *mirValue) {
        auto result = translateOperand(mirValue);
        if (auto reg = dynamic_cast<rRegister>(result)) {
            return reg;
        } else if (auto addr = dynamic_cast<rAddress>(result)) {
            return addr2reg(addr);
        }
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
    void translateFnegInst(const mir::Instruction::fneg *fnegInst);
    void translateAllocaInst(const mir::Instruction::alloca_ *allocaInst);
    void translateLoadInst(const mir::Instruction::load *loadInst);
    void translateStoreInst(const mir::Instruction::store *storeInst);
    void translateGetPtrInst(const mir::Instruction::getelementptr *getPtrInst);
    void translateConversionInst(const mir::Instruction::trunc *truncInst);
    void translateConversionInst(const mir::Instruction::zext *zextInst);
    void translateConversionInst(const mir::Instruction::sext *sextInst);
    template <mir::Instruction::InstrTy ty>
    void translateFpConvInst(const mir::Instruction::_conversion_instruction<ty> *fpConvInst);
    void translateIcmpInst(const mir::Instruction::icmp *icmpInst);
    void translateFcmpInst(const mir::Instruction::fcmp *fcmpInst);
    void translatePhiInst(const mir::Instruction::phi *phiInst);
    void translateSelectInst(const mir::Instruction::select *selectInst);
    void translateCallInst(const mir::Instruction::call *callInst);
    void translateMemsetInst(const mir::Instruction::memset *memsetInst);

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
