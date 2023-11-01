//
// Created by toby on 2023/11/8.
//

#include "reg_alloca.h"

namespace backend {
    void register_alloca(mips::rFunction function) {
        compute_blocks_info(function);
        // 3. alloca registers
        RegisterGraph graph(function);
        graph.compute_conflict();
        graph.compute_registers();
    }

    void compute_blocks_info(mips::rFunction function) {
        // 1. compute use and def
        for (auto &block: function->blocks)
            compute_use_def(block.get());
        // 2. compute liveIn and liveOut
        for (auto &block: function->blocks) {
            block->liveIn.insert(block->use.begin(), block->use.end());
            block->liveIn.erase(mips::PhyRegister::get(0));
        }
        while (compute_liveIn_liveOut(function));
    }

    void compute_use_def(mips::rBlock block) {
        for (auto &inst: block->allInstructions()) {
            for (auto reg: inst->regUse)
                if (block->def.count(reg) == 0)
                    block->use.insert(reg);
            for (auto reg: inst->regDef)
                if (block->use.count(reg) == 0)
                    block->def.insert(reg);
        }
    }

    bool compute_liveIn_liveOut(mips::rFunction function) {
        bool changed = false;
        for (auto it = function->blocks.rbegin(); it != function->blocks.rend(); it++) {
            auto block = it->get();
            auto s1 = block->liveIn.size();
            auto s2 = block->liveOut.size();
            for (auto suc: block->successors)
                block->liveOut.insert(suc->liveIn.begin(), suc->liveIn.end());
            block->liveIn.insert(block->liveOut.begin(), block->liveOut.end());
            for (auto reg: block->def)
                block->liveIn.erase(reg);
            if (s1 != block->liveIn.size() || s2 != block->liveOut.size())
                changed = true;
        }
        return changed;
    }

    void RegisterGraph::compute_conflict() {
        for (auto &bb: function->blocks) {
            vir_set_t temp{};
            auto f = [&temp](auto reg) {
                if (auto vir = dynamic_cast<mips::rVirRegister>(reg))
                    temp.insert(vir);
            };
            std::for_each(bb->liveIn.begin(), bb->liveIn.end(), f);
            std::for_each(bb->liveOut.begin(), bb->liveOut.end(), f);
            for (auto r1: temp)
                for (auto r2: temp)
                    if (r1 != r2)
                        conflict[r1].insert(r2);
            need_color.insert(temp.begin(), temp.end());
        }
    }

    void RegisterGraph::compute_registers() {
        // global
        color_registers(saved_regs);
        for (auto reg: need_color) {
            // -> $sx
            if (std::holds_alternative<mips::rPhyRegister>(v2p[reg]))
                reg->swapTo(std::get<mips::rPhyRegister>(v2p[reg]));
        }
        for (auto &bb: function->blocks) {
            // -> load / store
            for (auto reg: bb->liveIn)
                if (auto vir = dynamic_cast<mips::rVirRegister>(reg);
                        vir && std::holds_alternative<int>(v2p[vir])) {
                    auto r = function->newVirRegister();
                    load_at(bb.get(), bb->instructions.cbegin(), r, std::get<int>(v2p[vir]));
                    vir->swapUseTo(r, bb.get());
                }
            for (auto reg: bb->liveOut)
                if (auto vir = dynamic_cast<mips::rVirRegister>(reg);
                        vir && std::holds_alternative<int>(v2p[vir])) {
                    auto r = function->newVirRegister();
                    store_at(bb.get(), bb->instructions.cend(), r, std::get<int>(v2p[vir]));
                    vir->swapDefTo(r, bb.get());
                }
        }
        // local
        auto backup_size = function->allocaSize;
        auto max_size = function->allocaSize;
        for (auto &bb: function->blocks) {
            // the memory can be used by the next block
            function->allocaSize = backup_size;
            compute_registers(bb.get());
            max_size = std::max(max_size, function->allocaSize);
        }
        function->allocaSize = max_size;
    }

    void RegisterGraph::color_registers(const phy_set_t &could_use) {
        if (need_color.empty()) return;
        // NOLINTNEXTLINE
        auto dfs = [this, &could_use](auto &&self, mips::rVirRegister reg) -> void {
            auto colors = could_use;
            for (auto v: conflict[reg])
                if (std::holds_alternative<mips::rPhyRegister>(v2p[v]))
                    colors.erase(std::get<mips::rPhyRegister>(v2p[v]));
            if (colors.empty()) {
                function->allocaSize += 4;
                v2p[reg] = -static_cast<int>(function->allocaSize);
            } else {
                auto color = *colors.begin();
                if (color->isSaved()) function->shouldSave.insert(color);
                v2p[reg] = color;
            }
            for (auto v: conflict[reg])
                if (std::holds_alternative<std::monostate>(v2p[v]))
                    self(self, v);
        };
        dfs(dfs, *need_color.begin());
    }

    void RegisterGraph::compute_registers(mips::rBlock block) {
        std::unordered_map<mips::rVirRegister, inst_postion_t> last_use;
        for (auto &inst: block->allInstructions())
            inst->for_each_use_vreg([&](auto r) { last_use[r] = inst->node; });
        phy_set_t avail = temp_regs;
        mips::rPhyRegister memory_regs[] = {mips::PhyRegister::get("$t8"), mips::PhyRegister::get("$t9")};
        int memory_regs_id = 0;
        for (auto &inst: block->allInstructions()) {
            auto it = inst->isJumpBranch() ? block->instructions.end() : inst->node;
            inst->for_each_use_vreg([&, this](mips::rVirRegister vir) {
                if (std::holds_alternative<int>(v2p[vir])) {
                    // if the use reg should load from memory
                    load_at(block, it, memory_regs[memory_regs_id], std::get<int>(v2p[vir]));
                    vir->swapUseIn(memory_regs[memory_regs_id], inst.get());
                    // use $t8 and $t9 alternatively
                    memory_regs_id ^= 1;
                } else {
                    // use register directly
                    // should be safe here
                    auto phy = std::get<mips::rPhyRegister>(v2p[vir]);
                    vir->swapUseIn(phy, inst.get());
                    if (last_use[vir] == it) {
                        // the last use of the virtual register
                        avail.insert(phy);
                    }
                }
            });
            inst->for_each_def_vreg([&, this](mips::rVirRegister vir) {
                if (std::holds_alternative<std::monostate>(v2p[vir])) {
                    // haven't alloc register nor memory.
                    if (avail.empty()) {
                        // no registers left
                        function->allocaSize += 4;
                        v2p[vir] = -static_cast<int>(function->allocaSize);
                    } else {
                        // use register directly
                        auto phy = *avail.begin();
                        avail.erase(avail.begin());
                        vir->swapDefIn(phy, inst.get());
                        v2p[vir] = phy;
                    }
                } else if (std::holds_alternative<int>(v2p[vir])) {
                    // if the def reg should store to memory
                    auto temp_it = it;
                    store_at(block, ++temp_it, memory_regs[memory_regs_id], std::get<int>(v2p[vir]));
                    vir->swapDefIn(memory_regs[memory_regs_id], inst.get());
                    // use $t8 and $t9 alternatively
                    memory_regs_id ^= 1;
                } else {
                    // use register directly
                    // should be safe here
                    auto phy = std::get<mips::rPhyRegister>(v2p[vir]);
                    vir->swapDefIn(phy, inst.get());
                }
            });
        }
    }

    RegisterGraph::inst_postion_t
    RegisterGraph::load_at(mips::rBlock block, inst_postion_t it, mips::rRegister dst, int offset) {
        return block->insert(it, std::make_unique<mips::LoadInst>(
                mips::Instruction::Ty::LW, dst, mips::PhyRegister::get("$fp"), offset));
    }

    RegisterGraph::inst_postion_t
    RegisterGraph::store_at(mips::rBlock block, inst_postion_t it, mips::rRegister src, int offset) {
        return block->insert(it, std::make_unique<mips::StoreInst>(
                mips::Instruction::Ty::SW, src, mips::PhyRegister::get("$fp"), offset));
    }

    const RegisterGraph::phy_set_t RegisterGraph::saved_regs = []() {
        phy_set_t could_use{};
        for (int i = 0; i < 8; i++)
            could_use.insert(mips::PhyRegister::get("$s" + std::to_string(i)));
        assert(could_use.size() == 8);
        return could_use;
    }();

    const RegisterGraph::phy_set_t RegisterGraph::temp_regs = []() {
        phy_set_t could_use{};
        for (int i = 0; i < 8; i++)
            could_use.insert(mips::PhyRegister::get("$t" + std::to_string(i)));
        assert(could_use.size() == 8);
        return could_use;
    }();
}
