//
// Created by toby on 2023/12/15.
//

#ifndef COMPILER_FUNCTIONAL_H
#define COMPILER_FUNCTIONAL_H

#include "mir.h"

namespace mir {
void functionInline(Function *func);
void connectBlocks(Function *func);
void calcPure(Function *func);
}  // namespace mir
#endif  // COMPILER_FUNCTIONAL_H
