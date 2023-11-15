//
// Created by toby on 2023/11/16.
//

#include <algorithm>
#include "reg_alloca.h"
#include "backend_opt.h"

namespace backend {
    void clearDeadCode(mips::rFunction function) {
        bool changed = true;
        while (changed) {
            changed = false;
            compute_blocks_info(function);
            for (auto &block: all_sub_blocks(function)) {
                std::unordered_set<mips::rInstruction> dead = {};
                auto used = block->liveOut;
                auto addUsed = [&](auto &&inst) { inst->for_each_use_reg([&](auto &&reg) { used.insert(reg); }); };
                for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
                    auto &inst = *it;
                    if (inst->isJumpBranch() || inst->isSyscall() || inst->isStore())  { addUsed(inst); continue; }
                    assert(!inst->regDef.empty());
                    bool live = std::any_of(inst->regDef.begin(), inst->regDef.end(),
                                            [&](auto &&reg) -> bool {
                                                return used.count(reg) || dynamic_cast<mips::rPhyRegister>(reg);
                                            });
                    if (live) { addUsed(inst); continue; }
                    dead.insert(inst.get());
                    changed = true;
                }
                for (auto &inst: dead)
                    block->erase(inst->node);
            }
        }
    }

    void relocateBlock(mips::rFunction function) {
        constexpr auto next = [](auto &&block) -> mips::rBlock {
            if (auto label = block->backInst()->getJumpLabel();
                    label && std::holds_alternative<mips::rBlock>(label->parent))
                return std::get<mips::rBlock>(label->parent);
            return nullptr;
        };

        std::unordered_set<mips::rBlock> unordered, ordered;
        for (auto &block: *function)
            unordered.insert(block.get());
        for (auto &block: *function)
            unordered.erase(next(block));
        ordered.insert(function->exitB.get());

        std::list<mips::pBlock> result;
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
}
