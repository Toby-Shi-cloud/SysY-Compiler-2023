//
// Created by toby on 2023/11/16.
//

#ifndef COMPILER_BACKEND_OPT_H
#define COMPILER_BACKEND_OPT_H

#include <optional>
#include "../mips.h"

namespace mips {
inline std::optional<int> getLastImm(mips::rInstruction inst, mips::rRegister reg) {
    bool first = true;
    for (auto block_it = inst->parent->node;; --block_it) {
        auto it = first ? inst->node : (*block_it)->end();
        while (it != (*block_it)->begin()) {
            auto &&cur = *--it;
            if (auto phy = dynamic_cast<mips::rPhyRegister>(reg);
                cur->isFuncCall() && (!phy || !phy->isSaved())) {
                return std::nullopt;
            }
            if (std::find(cur->regDef.cbegin(), cur->regDef.cend(), reg) == cur->regDef.cend()) {
                continue;
            }
            if (cur->ty != mips::Instruction::Ty::LI && cur->ty != mips::Instruction::Ty::LUI) {
                return std::nullopt;
            }
            auto loadImm = dynamic_cast<mips::rBinaryIInst>(cur.get());
            assert(loadImm);
            if (loadImm->imm->isDyn()) return std::nullopt;
            if (loadImm->ty == mips::Instruction::Ty::LI) return loadImm->imm->value;
            return loadImm->imm->value << 16;
        }
        first = false;
        if (block_it == inst->parent->parent->subBlocks.begin()) break;
    }
    return std::nullopt;
}

inline std::pair<rRegister, rRegister> find_hi_lo(rBinaryMInst inst) {
    auto it = inst->node;
    rRegister hi = nullptr, lo = nullptr;
    if (inst->ty == Instruction::Ty::MUL) lo = inst->dst();
    if (++it == inst->parent->end()) return {hi, lo};
    if ((*it)->ty == Instruction::Ty::MFHI)
        hi = (*it)->regDef[0];
    else if ((*it)->ty == Instruction::Ty::MFLO)
        lo = (*it)->regDef[0];
    else
        return {hi, lo};
    if (++it == inst->parent->end()) return {hi, lo};
    if ((*it)->ty == Instruction::Ty::MFHI)
        hi = (*it)->regDef[0];
    else if ((*it)->ty == Instruction::Ty::MFLO)
        lo = (*it)->regDef[0];
    return {hi, lo};
};
}  // namespace mips

namespace mips {
// Clear instructions that are translated but not used
void clearDeadCode(mips::rFunction function);

// Relocate all blocks to reduce the number of jumps
void relocateBlock(mips::rFunction function);

// Flod div & rem with same operands. (i.e. a/b, a%b can be flodded into one div instruction)
void divisionFold(mips::rFunction function);

// Convert x / imm (or x % imm) to multiplication (if possible)
void div2mul(mips::rFunction function);

// Clear duplicate instructions (LI/LUI)
void clearDuplicateInst(mips::rFunction function);

// Do some arithmetic folding
void arithmeticFolding(mips::rFunction function);
}  // namespace mips

#endif  // COMPILER_BACKEND_OPT_H
