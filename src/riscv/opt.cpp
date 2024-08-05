//
// Created by toby on 2024/8/5.
//

#include "riscv/opt.h"
#include "riscv/reg_alloca.h"
#include "util.h"

namespace backend::riscv {
void clearDeadCode(rFunction function) {
    bool changed = true;
    while (changed) {
        changed = false;
        compute_blocks_info(function);
        for (auto &block : all_sub_blocks(function)) {
            std::vector<rInstruction> dead = {};
            auto used = block->liveOut;
            auto pred = [&](auto &&reg) -> bool {
                static const auto zero = "x0"_R;
                return reg != zero && used.count(reg);
            };
            auto addUsed = [&](auto &&inst) {
                std::for_each(inst->regUse.begin(), inst->regUse.end(),
                              [&](auto &&reg) { used.insert(reg); });
            };
            auto earseDef = [&](auto &&inst) {
                std::for_each(inst->regDef.begin(), inst->regDef.end(),
                              [&](auto &&reg) { used.erase(reg); });
            };
            for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
                auto inst = dynamic_cast<Instruction *>(it->get());
                assert(inst != nullptr);
                if (inst->isJumpBranch() || inst->isStore()) {
                    earseDef(inst);
                    addUsed(inst);
                    continue;
                }
                assert(!inst->regDef.empty());
                if (std::any_of(inst->regDef.begin(), inst->regDef.end(), pred)) {
                    earseDef(inst);
                    addUsed(inst);
                    continue;
                }
                earseDef(inst);
                dead.push_back(inst);
                changed = true;
            }
            for (auto &inst : dead) block->erase(inst->node);
        }
    }
}

void relocateBlock(rFunction function) {
    constexpr auto next = [](auto &&block) -> rBlock {
        if (auto label = block->backInst()->getJumpLabel();
            label && std::holds_alternative<rBlock>(label->parent))
            return std::get<rBlock>(label->parent);
        return nullptr;
    };

    std::unordered_set<rBlock> unordered, ordered;
    for (auto &block : *function) unordered.insert(block.get());
    for (auto &block : *function) unordered.erase(next(block));
    ordered.insert(function->exitB.get());

    std::list<pBlock> result;
    for (auto current = function->begin()->get(); !unordered.empty();
         current = unordered.empty() ? nullptr : *unordered.begin()) {
        unordered.erase(current);
        while (current && !ordered.count(current)) {
            ordered.insert(current);
            current->node = result.insert(result.cend(), std::move(*current->node));
            current = next(current);
        }
    }
    function->blocks = std::move(result);
}

void divisionFold(rFunction function) { TODO("divisionFold"); }

static std::pair<unsigned, int> div2mul(unsigned divisor) {
    for (int l = 0; l < 32; l++) {
        constexpr int N = 32;
        auto p1 = 1LLU << (N + l);
        auto m = (p1 + (1LLU << l)) / divisor;
        if (m > UINT32_MAX) continue;
        if (m * divisor < p1) continue;
        return {static_cast<unsigned>(m), l};
    }
    return {};
}

void div2mul(rFunction function) { TODO("div2mul"); }
}  // namespace backend::riscv
