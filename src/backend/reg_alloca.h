//
// Created by toby on 2023/11/8.
//

#ifndef COMPILER_REG_ALLOCA_H
#define COMPILER_REG_ALLOCA_H

#include <set>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include "../mips.h"

namespace backend {
    void register_alloca(mips::rFunction function);

    void compute_blocks_info(mips::rFunction function);

    void compute_use_def(mips::rBlock block);

    bool compute_liveIn_liveOut(mips::rFunction function);

    class RegisterGraph {
        mips::rFunction function;

        using phy_position_t = std::variant<std::monostate, mips::rPhyRegister, int>;
        std::unordered_map<mips::rVirRegister, phy_position_t> v2p;

        using vir_set_t = std::set<mips::rVirRegister>;
        using phy_set_t = std::set<mips::rPhyRegister>;
        std::unordered_set<mips::rVirRegister> need_color;
        std::unordered_map<mips::rVirRegister, vir_set_t> conflict;

        static const phy_set_t saved_regs;

        static const phy_set_t temp_regs;

        void color_registers(const phy_set_t &could_use);

        void compute_registers(mips::rBlock block);

        using inst_postion_t = std::list<mips::pInstruction>::const_iterator;

        static inst_postion_t load_at(mips::rBlock block, inst_postion_t it, mips::rRegister dst, int offset);

        static inst_postion_t store_at(mips::rBlock block, inst_postion_t it, mips::rRegister src, int offset);

    public:
        explicit RegisterGraph(mips::rFunction function) : function(function) {}

        void compute_conflict();

        void compute_registers();
    };
}

#endif //COMPILER_REG_ALLOCA_H
