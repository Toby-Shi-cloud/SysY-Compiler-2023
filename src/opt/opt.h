//
// Created by toby on 2023/11/24.
//

#ifndef COMPILER_OPT_H
#define COMPILER_OPT_H

#include "mem2reg.h"
#include "functional.h"

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

    template<typename... Args>
    auto substitute(Instruction *_old, Instruction *_new, Args... args) {
        auto bb = _old->parent;
        bb->insert(_old->node, _new);
        return substitute(_old, std::forward<Args>(args)...);
    }

    template<typename T>
    std::enable_if_t<std::is_base_of_v<Instruction, T>, inst_node_t>
    constantFolding(T *inst) {
        extern inst_node_t constantFolding(T *);
        return constantFolding(inst);
    }

    inline bool isPureInst(const Instruction *inst) {
        if (inst->isMemoryAccess()) return false;
        if (auto call = dynamic_cast<const Instruction::call *>(inst))
            return call->getFunction()->isPure;
        return true;
    }

    using root_value_t = std::pair<const Value *, std::optional<int>>;
    inline root_value_t getRootValue(const Value *value) {
        if (auto load = dynamic_cast<const Instruction::load *>(value))
            return getRootValue(load->getPointerOperand());
        if (auto store = dynamic_cast<const Instruction::store *>(value))
            return getRootValue(store->getDest());
        if (auto gep = dynamic_cast<const Instruction::getelementptr *>(value)) {
            auto ret = getRootValue(gep->getPointerOperand());
            if (!ret.second) return ret;
            try {
                auto index = gep->getIndexOffset();
                *ret.second += index;
            } catch (std::out_of_range &) {
                ret.second = std::nullopt;
            }
            return ret;
        }
        return {value, 0};
    }

    void constantFolding(const Function *func);

    void globalVariableNumbering(const Function *func);

    void globalCodeMotion(Function *func);

    void spiltArray(Manager &mgr);

    void inlineGlobalVar(Manager &mgr);
}

namespace mir {
#define DECLARE(field, id) \
    [[nodiscard]] int &field() { return (*this)[id]; } \
    [[nodiscard]] int field() const { return (*this)[id]; }

    inline struct OptInfos : std::array<int, 9> {
        DECLARE(mem_to_reg, 0)
        DECLARE(constant_folding, 1)
        DECLARE(global_variable_numbering, 2)
        DECLARE(clear_dead_inst, 3)
        DECLARE(clear_dead_block, 4)
        DECLARE(merge_empty_block, 5)
        DECLARE(function_inline, 6)
        DECLARE(split_array, 7)
        DECLARE(inline_global_var, 8)

        OptInfos operator+(const OptInfos &other) const {
            OptInfos res = {};
            for (int i = 0; i < size(); ++i) res[i] = (*this)[i] + other[i];
            return res;
        }

        OptInfos &operator+=(const OptInfos &other) {
            for (int i = 0; i < size(); ++i) (*this)[i] += other[i];
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
        stream << str(mem_to_reg) << ", ";
        stream << str(constant_folding) << ", ";
        stream << str(global_variable_numbering) << ", ";
        stream << str(clear_dead_inst) << ", ";
        stream << str(clear_dead_block) << ", ";
        stream << str(merge_empty_block) << ", ";
        stream << str(function_inline) << ", ";
        stream << str(split_array) << ", ";
        stream << str(inline_global_var);
        stream << "}";
        return true;
#undef str
    }
}
#endif

#endif //COMPILER_OPT_H
