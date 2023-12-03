//
// Created by toby on 2023/11/14.
//

#include "opt.h"
#include "../settings.h"

namespace mir {
    static void basic_optimize(Function *func) {
// this macro is used to allocName for values when debug mod enabled.
#define FUNC (assert((func->allocName(), true)), func)
        if (opt_settings.using_mem2reg) mem2reg(FUNC);
        if (!opt_settings.force_no_opt) clearDeadBlock(FUNC);
        if (!opt_settings.force_no_opt) clearDeadInst(FUNC);
        if (opt_settings.using_constant_folding) constantFolding(FUNC);
        if (opt_settings.using_gcm) globalCodeMotion(FUNC);
        if (opt_settings.using_lvn) globalVariableNumbering(FUNC);
        if (opt_settings.using_block_merging) mergeEmptyBlock(FUNC);
        if (opt_settings.using_block_merging) connectBlocks(FUNC);
#undef FUNC
    }

    void Manager::optimize() {
        OptInfos _sum = {};
        opt_infos = { 1 };
        while (opt_infos != OptInfos{}) {
            opt_infos = {};
            for_each_func(basic_optimize);
            if (opt_infos == OptInfos{})
                if (opt_settings.using_force_inline)
                    for_each_func(functionInline);
            clearUnused();
            _sum += opt_infos;
        }
        dbg(_sum);
    }

    void Manager::clearUnused() {
        for (auto it = functions.begin(); it != functions.end();) {
            if (auto &&func = *it; func->getName() != "@main" && !func->isUsed()) {
                delete func;
                it = functions.erase(it);
            } else ++it;
        }

        for (auto it = globalVars.begin(); it != globalVars.end();) {
            if (auto &&var = *it; !var->isUsed()) {
                if (auto str = dynamic_cast<StringLiteral *>(var->init))
                    stringPool.erase(str->value);
                delete var;
                it = globalVars.erase(it);
            } else ++it;
        }
    }


    void Function::clearBBInfo() const {
        for (auto bb: bbs)
            bb->clear_info();
        exitBB->clear_info();
    }

    void Function::calcPreSuc() const {
        constexpr auto link = [](auto pre, auto suc) {
            pre->successors.insert(suc);
            suc->predecessors.insert(pre);
        };

        clearBBInfo();
        for (auto bb: bbs) {
            if (auto inst = *bb->instructions.crbegin();
                inst->instrTy == Instruction::RET) {
                link(bb, exitBB);
            } else if (auto br = dynamic_cast<Instruction::br *>(inst)) {
                if (br->hasCondition()) {
                    link(bb, br->getIfTrue());
                    link(bb, br->getIfFalse());
                } else {
                    link(bb, br->getTarget());
                }
            } else {
                assert(!"The last instruction in a basic block should be 'br' or 'ret'");
            }
        }
    }
}
