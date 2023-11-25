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

namespace mir {
#define DECLARE(field, id) \
    [[nodiscard]] int &field() { return (*this)[id]; } \
    [[nodiscard]] int field() const { return (*this)[id]; }

    inline struct OptInfos : std::array<int, 5> {
        DECLARE(constant_folding, 0)
        DECLARE(clear_dead_inst, 1)
        DECLARE(clear_dead_block, 2)
        DECLARE(merge_empty_block, 3)
        DECLARE(mem_to_reg, 4)

        OptInfos operator+(const OptInfos &other) const {
            OptInfos res = {};
            for (int i = 0; i < 5; ++i) res[i] = (*this)[i] + other[i];
            return res;
        }

        OptInfos &operator+=(const OptInfos &other) {
            for (int i = 0; i < 5; ++i) (*this)[i] += other[i];
            return *this;
        }
    } opt_infos;

#undef DECLARE
}

#ifdef DBG_ENABLE
namespace dbg {
    template<>
    inline bool pretty_print(std::ostream &stream, const mir::OptInfos &value) {
#define str(field) #field << ": " << value.field()
        stream << "{";
        stream << str(constant_folding) << ", ";
        stream << str(clear_dead_inst) << ", ";
        stream << str(clear_dead_block) << ", ";
        stream << str(merge_empty_block) << ", ";
        stream << str(mem_to_reg);
        stream << "}";
        return true;
#undef str
    }
}
#endif

#endif //COMPILER_OPT_H
