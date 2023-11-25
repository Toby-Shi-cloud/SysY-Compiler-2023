//
// Created by toby on 2023/11/24.
//

#ifndef COMPILER_OPT_H
#define COMPILER_OPT_H

#include "mem2reg.h"

namespace mir {
    inline auto substitute(Instruction *_old, Instruction *_new) {
        auto bb = _old->parent;
        _old->moveTo(_new);
        bb->insert(_old->node, _new);
        return bb->erase(_old);
    }

    inline auto substitute(Instruction *_old, Value *_new) {
        _old->moveTo(_new);
        return _old->parent->erase(_old);
    }

    inst_node_t constantFolding(Instruction *inst);

    void constantFolding(const Function *func);
}

#endif //COMPILER_OPT_H
