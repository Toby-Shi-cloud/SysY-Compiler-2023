//
// Created by toby on 2023/11/14.
//

#include "mem2reg.h"

namespace mir {
    void Function::clearBBInfo() {
        for (auto bb: bbs)
            bb->clear_info();
        exitBB->clear_info();
    }

    void Function::calcPreSuc() {
        constexpr auto link = [](auto pre, auto suc) {
            pre->successors.insert(suc);
            suc->predecessors.insert(pre);
        };

        clearBBInfo();
        for (auto bb: bbs) {
            auto inst = *bb->instructions.crbegin();
            if (inst->instrTy == Instruction::RET) {
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

    void Manager::optimize() {
        assert((allocName(), true));
        for_each_func(clearDeadInst);
        for_each_func(reCalcBBInfo);
        for_each_func(calcPhi);
        for_each_func(clearDeadInst);
    }
}
