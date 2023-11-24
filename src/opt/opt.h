//
// Created by toby on 2023/11/24.
//

#ifndef COMPILER_OPT_H
#define COMPILER_OPT_H

#include "mem2reg.h"

namespace mir {
    inline auto substitude(Instruction *_old, Instruction *_new) {
        auto bb = _old->parent;
        if (_new->isUsed()) _old->moveTo(_new);
        else _old->swap(_new);
        bb->insert(_old->node, _new);
        return bb->erase(_old);
    }

    inline auto substitude(Instruction *_old, Value *_new) {
        if (_new->isUsed()) _old->moveTo(_new);
        else _old->swap(_new);
        return _old->parent->erase(_old);
    }

    void constantFolding(const Function *function);
}

#endif //COMPILER_OPT_H
