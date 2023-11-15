//
// Created by toby on 2023/11/16.
//

#ifndef COMPILER_BACKEND_OPT_H
#define COMPILER_BACKEND_OPT_H

#include "../mips.h"

namespace backend {
    void clearDeadCode(mips::rFunction function);

    void relocateBlock(mips::rFunction function);
}

#endif //COMPILER_BACKEND_OPT_H
