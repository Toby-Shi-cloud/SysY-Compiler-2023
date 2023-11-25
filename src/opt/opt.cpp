//
// Created by toby on 2023/11/14.
//

#include "opt.h"

namespace mir {
    static void _optimize(Function *func) {
// this macro is used to allocName for values when debug mod enabled.
#define FUNC (assert((func->allocName(), true)), func)
        constantFolding(FUNC);
        clearDeadInst(FUNC);
        clearDeadBlock(FUNC);
        mergeEmptyBlock(FUNC);
        calcPhi(FUNC);
#undef FUNC
    }

    void Manager::optimize() {
        OptInfos _sum = {};
        opt_infos = { 1 };
        while (opt_infos != OptInfos{}) {
            opt_infos = {};
            for_each_func(_optimize);
            _sum += opt_infos;
        }
        dbg(_sum);

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
                link(bb, exitBB.get());
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
