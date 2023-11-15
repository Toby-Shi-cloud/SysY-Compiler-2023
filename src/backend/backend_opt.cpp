//
// Created by toby on 2023/11/16.
//

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
                    dbg(inst);
                    dead.insert(inst.get());
                    changed = true;
                }
                for (auto &inst: dead)
                    block->erase(inst->node);
            }
        }
    }
}
