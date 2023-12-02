//
// Created by toby on 2023/11/14.
//

#ifndef COMPILER_MEM2REG_H
#define COMPILER_MEM2REG_H

#include "../mir.h"

namespace mir {
    void calcPhi(Function *func);

    void clearDeadInst(const Function *func);

    void clearDeadBlock(Function *func);

    void mergeEmptyBlock(Function *func);
}

#endif //COMPILER_MEM2REG_H
