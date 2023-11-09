//
// Created by toby on 2023/11/8.
//

#include <algorithm>
#include "../mips.h"

namespace mips {
    void Register::swapDefTo(mips::rRegister other, mips::rBlock block) {
        std::unordered_set<rInstruction> temp{};
        for (auto inst: defUsers) {
            if (block && inst->parent != block) continue;
            for (auto &r: inst->regDef)
                if (r == this) r = other, temp.insert(inst);
        }
        other->defUsers.insert(temp.begin(), temp.end());
        std::for_each(temp.begin(), temp.end(), [this](auto &&x) { defUsers.erase(x); });
    }

    void Register::swapUseTo(mips::rRegister other, mips::rBlock block) {
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
}

namespace mips {
    rLabel Block::nextLabel() const {
        if (node == block_node_t{}) return nullptr;
        auto it = node;
        if (++it == parent->end()) return parent->exitB->label.get();
        return (*it)->label.get();
    }

    rBlock Block::splice(inst_node_t pos) {
        if (pos == instructions.end()) return nullptr;
        assert(pos->get()->isFuncCall());
        auto nxt = pos;
        if (++nxt == instructions.end() && !conditionalJump) {
            conditionalJump = std::move(*pos);
            instructions.erase(pos);
            return nullptr;
        }
        auto nBlock = new Block(parent);
        nBlock->instructions.splice(nBlock->instructions.begin(), instructions, nxt, instructions.end());
        nBlock->conditionalJump = std::move(conditionalJump);
        nBlock->fallthroughJump = std::move(fallthroughJump);
        for (auto &inst: nBlock->instructions) inst->parent = nBlock;
        conditionalJump = std::move(*pos);
        instructions.erase(pos);
        fallthroughJump = std::make_unique<JumpInst>(Instruction::Ty::J, nBlock->label.get());
        return nBlock;
    }

    void Block::merge(mips::Block *block) {
        assert(conditionalJump == nullptr);
        assert(fallthroughJump->getJumpLabel() == block->label.get());
        for (auto &inst: block->instructions)
            push_back(make_copy(inst));
        conditionalJump = make_copy(block->conditionalJump);
        fallthroughJump = make_copy(block->fallthroughJump);
        if (block != parent->exitB.get()) {
            successors.erase(block);
            block->predecessors.erase(this);
            for (auto suc: block->successors)
                successors.insert(suc);
        }
    }
}
