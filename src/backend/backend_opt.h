//
// Created by toby on 2023/11/16.
//

#ifndef COMPILER_BACKEND_OPT_H
#define COMPILER_BACKEND_OPT_H

#include <optional>
#include "mips.h"

namespace backend::mips {
inline std::optional<int> getLastImm(rInstruction inst, rRegister reg) {
    bool first = true;
    for (auto block_it = inst->parent->node;; --block_it) {
        auto it = first ? inst->node : (*block_it)->end();
        while (it != (*block_it)->begin()) {
            auto &&cur = dynamic_cast<Instruction *>((--it)->get());
            assert(cur != nullptr);
            if (auto phy = dynamic_cast<rPhyRegister>(reg);
                cur->isFuncCall() && (!phy || !phy->isSaved())) {
                return std::nullopt;
            }
            if (std::find(cur->regDef.cbegin(), cur->regDef.cend(), reg) == cur->regDef.cend()) {
                continue;
            }
            if (cur->ty != Instruction::Ty::LI && cur->ty != Instruction::Ty::LUI) {
                return std::nullopt;
            }
            auto loadImm = dynamic_cast<rBinaryIInst>(cur);
            assert(loadImm);
            if (loadImm->imm->isDyn()) return std::nullopt;
            if (loadImm->ty == Instruction::Ty::LI) return loadImm->imm->value;
            return loadImm->imm->value << 16;
        }
        first = false;
        if (block_it == inst->parent->parent->subBlocks.begin()) break;
    }
    return std::nullopt;
}

inline std::pair<rRegister, rRegister> find_hi_lo(rBinaryMInst inst) {
    auto it = inst->node;
    auto cur = dynamic_cast<Instruction *>(it->get());
    rRegister hi = nullptr, lo = nullptr;
    if (inst->ty == Instruction::Ty::MUL) lo = inst->dst();
    if (++it == inst->parent->end()) return {hi, lo};
    if (cur->ty == Instruction::Ty::MFHI)
        hi = cur->regDef[0];
    else if (cur->ty == Instruction::Ty::MFLO)
        lo = cur->regDef[0];
    else
        return {hi, lo};
    if (++it == inst->parent->end()) return {hi, lo};
    if (cur->ty == Instruction::Ty::MFHI)
        hi = cur->regDef[0];
    else if (cur->ty == Instruction::Ty::MFLO)
        lo = cur->regDef[0];
    return {hi, lo};
};
}  // namespace backend::mips

namespace backend::mips {
// Clear instructions that are translated but not used
void clearDeadCode(rFunction function);

// Relocate all blocks to reduce the number of jumps
void relocateBlock(rFunction function);

// Flod div & rem with same operands. (i.e. a/b, a%b can be flodded into one div instruction)
void divisionFold(rFunction function);

// Convert x / imm (or x % imm) to multiplication (if possible)
void div2mul(rFunction function);

// Clear duplicate instructions (LI/LUI)
void clearDuplicateInst(rFunction function);

// Do some arithmetic folding
void arithmeticFolding(rFunction function);
}  // namespace backend::mips

#endif  // COMPILER_BACKEND_OPT_H
