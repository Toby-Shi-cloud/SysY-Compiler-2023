//
// Created by toby on 2023/11/8.
//

#include "backend/component.h"
#include <algorithm>

namespace backend {
void Register::swapDefTo(rRegister other, rSubBlock block) {
    std::unordered_set<rInstructionBase> temp{};
    for (auto inst : defUsers) {
        if (block && inst->parent != block) continue;
        for (auto &r : inst->regDef)
            if (r == this) r = other, temp.insert(inst);
    }
    other->defUsers.insert(temp.begin(), temp.end());
    std::for_each(temp.begin(), temp.end(), [this](auto &&x) { defUsers.erase(x); });
}

void Register::swapUseTo(rRegister other, rSubBlock block) {
    std::unordered_set<rInstructionBase> temp{};
    for (auto inst : useUsers) {
        if (block && inst->parent != block) continue;
        for (auto &r : inst->regUse)
            if (r == this) r = other, temp.insert(inst);
    }
    other->useUsers.insert(temp.begin(), temp.end());
    std::for_each(temp.begin(), temp.end(), [this](auto &&x) { useUsers.erase(x); });
}

void Register::swapDefIn(rRegister other, rInstructionBase inst) {
    if (defUsers.count(inst) == 0) return;
    for (auto &r : inst->regDef)
        if (r == this) r = other;
    defUsers.erase(inst);
    other->defUsers.insert(inst);
}

void Register::swapUseIn(rRegister other, rInstructionBase inst) {
    if (useUsers.count(inst) == 0) return;
    for (auto &r : inst->regUse)
        if (r == this) r = other;
    useUsers.erase(inst);
    other->useUsers.insert(inst);
}

rBlock Block::splitBlock(inst_pos_t pos) {
    assert((*pos)->parent->parent == this);
    auto oldSubBlock = (*pos)->parent;
    auto newBlock = new Block(parent);
    newBlock->frontBlock()->instructions.splice(newBlock->frontBlock()->end(),
                                                oldSubBlock->instructions, pos, oldSubBlock->end());
    for (auto it = pos; it != newBlock->frontBlock()->end(); ++it)
        (*it)->parent = newBlock->frontBlock();
    newBlock->subBlocks.splice(newBlock->subBlocks.end(), subBlocks, std::next(oldSubBlock->node),
                               subBlocks.end());
    for (auto &&sub : newBlock->subBlocks) sub->parent = newBlock;
    newBlock->node = parent->blocks.emplace(std::next(node), newBlock);
    return newBlock;
}

rLabel Block::nextLabel() const {
    if (node == block_node_t{}) return nullptr;
    auto it = node;
    if (++it == parent->end()) return parent->exitB->label.get();
    return (*it)->label.get();
}

void Block::clearBlockInfo() const {
    for (auto &sub : subBlocks) sub->clearInfo();
}

void Block::computePreSuc() const {
    clearBlockInfo();
    constexpr auto link = [](auto &&x, auto &&y) {
        x->successors.insert(y);
        y->predecessors.insert(x);
    };

    rSubBlock lst = nullptr;
    for (auto &sub : subBlocks) {
        auto lbl = sub->back()->getJumpLabel();
        if (lbl == nullptr) continue;
        if (std::holds_alternative<rBlock>(lbl->parent)) {
            auto suc = std::get<rBlock>(lbl->parent);
            link(sub.get(), suc->frontBlock());
        }
        if (lst) link(lst, sub.get());
        lst = sub.get();
    }
}

void Module::calcGlobalVarOffset() const {
    int offset = -0x8000;
    for (auto &&var : globalVars) {
        if (var->isInit) continue;
        if (static_cast<int>(var->size + offset) >= 0x8000) continue;
        var->inExtern = true;
        var->offsetofGp = offset;
        offset += static_cast<int>(var->size);
    }
}

rInstructionBase InstructionBase::next() const {
    auto it = node;
    if (++it == parent->end()) return nullptr;
    return it->get();
}
}  // namespace backend
