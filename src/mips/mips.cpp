//
// Created by toby on 2023/11/8.
//

#include <algorithm>
#include "../mips.h"

namespace mips {
    void Register::swapDefTo(mips::rRegister other, mips::rSubBlock block) {
        std::unordered_set<rInstruction> temp{};
        for (auto inst: defUsers) {
            if (block && inst->parent != block) continue;
            for (auto &r: inst->regDef)
                if (r == this) r = other, temp.insert(inst);
        }
        other->defUsers.insert(temp.begin(), temp.end());
        std::for_each(temp.begin(), temp.end(), [this](auto &&x) { defUsers.erase(x); });
    }

    void Register::swapUseTo(mips::rRegister other, mips::rSubBlock block) {
        std::unordered_set<rInstruction> temp{};
        for (auto inst: useUsers) {
            if (block && inst->parent != block) continue;
            for (auto &r: inst->regUse)
                if (r == this) r = other, temp.insert(inst);
        }
        other->useUsers.insert(temp.begin(), temp.end());
        std::for_each(temp.begin(), temp.end(), [this](auto &&x) { useUsers.erase(x); });
    }

    void Register::swapDefIn(mips::rRegister other, mips::rInstruction inst) {
        if (defUsers.count(inst) == 0) return;
        for (auto &r: inst->regDef)
            if (r == this) r = other;
        defUsers.erase(inst);
        other->defUsers.insert(inst);
    }

    void Register::swapUseIn(mips::rRegister other, mips::rInstruction inst) {
        if (useUsers.count(inst) == 0) return;
        for (auto &r: inst->regUse)
            if (r == this) r = other;
        useUsers.erase(inst);
        other->useUsers.insert(inst);
    }

    rLabel Block::nextLabel() const {
        if (node == block_node_t{}) return nullptr;
        auto it = node;
        if (++it == parent->end()) return parent->exitB->label.get();
        return (*it)->label.get();
    }

    void Block::clearBlockInfo() const {
        for (auto &sub: subBlocks)
            sub->clearInfo();
    }

    void Block::computePreSuc() const {
        clearBlockInfo();
        constexpr auto link = [](auto &&x, auto &&y) {
            x->successors.insert(y);
            y->predecessors.insert(x);
        };

        rSubBlock lst = nullptr;
        for (auto &sub: subBlocks) {
            auto lbl = sub->back()->getJumpLabel();
            if (lbl == nullptr) continue;
            if (std::holds_alternative<mips::rBlock>(lbl->parent)) {
                auto suc = std::get<mips::rBlock>(lbl->parent);
                link(sub.get(), suc->frontBlock());
            }
            if (lst) link(lst, sub.get());
            lst = sub.get();
        }
    }
}
