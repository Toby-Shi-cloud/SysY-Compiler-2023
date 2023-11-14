//
// Created by toby on 2023/11/14.
//

#ifndef COMPILER_MEM2REG_H
#define COMPILER_MEM2REG_H

#include "../mir.h"

namespace mir {
    void reCalcBBInfo(Function *func);

    void calcDominators(Function *func);

    void calcDF(Function *func);

    void calcPhi(Function *func);

    void clearDeadInst(Function *func);

    void clearDeadBlock(Function *func);
}

#endif //COMPILER_MEM2REG_H
