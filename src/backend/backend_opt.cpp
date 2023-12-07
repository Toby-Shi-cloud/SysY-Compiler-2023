//
// Created by toby on 2023/11/16.
//

#include <map>
#include <stack>
#include <algorithm>
#include <unordered_set>
#include "reg_alloca.h"
#include "backend_opt.h"

namespace backend {
    inline void clearDeadBlock(mips::rFunction function) {
        std::unordered_set<mips::rBlock> visited;
        std::stack<mips::rBlock> stack;
        visited.insert(function->blocks.front().get());
        stack.push(function->blocks.front().get());
        while (!stack.empty()) {
            auto block = stack.top();
            stack.pop();
            for (auto &sub: block->subBlocks) {
                auto lbl = sub->back()->getJumpLabel();
                if (lbl == nullptr) continue;
                if (!std::holds_alternative<mips::rBlock>(lbl->parent)) continue;
                auto suc = std::get<mips::rBlock>(lbl->parent);
                if (suc == function->exitB.get()) continue;
                if (visited.count(suc)) continue;
                stack.push(suc);
                visited.insert(suc);
            }
        }
        for (auto it = function->begin(); it != function->end();) {
            auto &&block = *it;
            if (!visited.count(block.get())) {
                it = function->blocks.erase(it);
            } else ++it;
        }
    }

    void clearDeadCode(mips::rFunction function) {
        bool changed = true;
        while (changed) {
            changed = false;
            compute_blocks_info(function);
            for (auto &block: all_sub_blocks(function)) {
                std::vector<mips::rInstruction> dead = {};
                auto used = block->liveOut;
                auto pred = [&](auto &&reg) -> bool {
                    if (auto phy = dynamic_cast<mips::rPhyRegister>(reg);
                        !phy || phy->isTemp() || phy->isSaved() ||
                        phy == mips::PhyRegister::HI || phy == mips::PhyRegister::LO)
                        return used.count(reg);
                    return true;
                };
                auto addUsed = [&](auto &&inst) {
                    std::for_each(inst->regUse.begin(), inst->regUse.end(), [&](auto &&reg) { used.insert(reg); });
                };
                auto earseDef = [&](auto &&inst) {
                    std::for_each(inst->regDef.begin(), inst->regDef.end(), [&](auto &&reg) { used.erase(reg); });
                };
                for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
                    auto &inst = *it;
                    if (inst->isJumpBranch() || inst->isSyscall() || inst->isStore()) {
                        addUsed(inst);
                        continue;
                    }
                    assert(!inst->regDef.empty());
                    if (std::any_of(inst->regDef.begin(), inst->regDef.end(), pred)) {
                        earseDef(inst);
                        addUsed(inst);
                        continue;
                    }
                    earseDef(inst);
                    dead.push_back(inst.get());
                    changed = true;
                }
                for (auto &inst: dead)
                    block->erase(inst->node);
            }
        }
    }

    void relocateBlock(mips::rFunction function) {
        constexpr auto next = [](auto &&block) -> mips::rBlock {
            if (auto label = block->backInst()->getJumpLabel();
                label && std::holds_alternative<mips::rBlock>(label->parent))
                return std::get<mips::rBlock>(label->parent);
            return nullptr;
        };

        std::unordered_set<mips::rBlock> unordered, ordered;
        for (auto &block: *function)
            unordered.insert(block.get());
        for (auto &block: *function)
            unordered.erase(next(block));
        ordered.insert(function->exitB.get());

        std::list<mips::pBlock> result;
        for (auto current = function->begin()->get(); !unordered.empty();
             current = unordered.empty() ? nullptr : *unordered.begin()) {
            unordered.erase(current);
            while (current && !ordered.count(current)) {
                ordered.insert(current);
                current->node = result.insert(result.cend(), std::move(*current->node));
                current = next(current);
            }
        }
        function->blocks = std::move(result);
    }

    void divisionFold(mips::rFunction function) {
        enum FoldType { NONE, MULU, DIV, DIVU };
        constexpr auto deduce_fold_type = [](auto ty) {
            switch (ty) {
                case mips::Instruction::Ty::MULTU: return MULU;
                case mips::Instruction::Ty::DIV: return DIV;
                case mips::Instruction::Ty::DIVU: return DIVU;
                default: return NONE;
            }
        };
        constexpr auto find_hi_lo = [](mips::pInstruction &inst) {
            auto it = inst->node;
            mips::rRegister hi = nullptr, lo = nullptr;
            if (++it == inst->parent->end()) return std::make_pair(hi, lo);
            if ((*it)->ty == mips::Instruction::Ty::MFHI) hi = (*it)->regDef[0];
            else if ((*it)->ty == mips::Instruction::Ty::MFLO) lo = (*it)->regDef[0];
            else return std::make_pair(hi, lo);
            if (++it == inst->parent->end()) return std::make_pair(hi, lo);
            if ((*it)->ty == mips::Instruction::Ty::MFHI) hi = (*it)->regDef[0];
            else if ((*it)->ty == mips::Instruction::Ty::MFLO) lo = (*it)->regDef[0];
            return std::make_pair(hi, lo);
        };

        for (auto &&block: *function) {
            std::map<std::tuple<FoldType, mips::rRegister, mips::rRegister>,
                std::tuple<mips::rInstruction, mips::rRegister, mips::rRegister>> map;
            for (auto &&sub: block->subBlocks) {
                for (auto &&inst: *sub) {
                    auto dt = deduce_fold_type(inst->ty);
                    if (dt == NONE) continue;
                    auto mInst = dynamic_cast<mips::rBinaryMInst>(inst.get());
                    assert(mInst);
                    auto identity = std::make_tuple(dt, mInst->src1(), mInst->src2());
                    auto [hi, lo] = find_hi_lo(inst);
                    if (map.count(identity)) {
                        auto &&result = map[identity];
                        auto &&[result_inst, result_hi, result_lo] = result;
                        static_assert(std::is_lvalue_reference_v<decltype(result)>, "wtf?");
                        if (hi) {
                            if (!result_hi) {
                                auto vir = function->newVirRegister();
                                result_inst->parent->insert(
                                    std::next(result_inst->node),
                                    std::make_unique<mips::MoveInst>(vir, mips::PhyRegister::HI));
                                result_hi = vir;
                            }
                            hi->swapUseTo(result_hi);
                        }
                        if (lo) {
                            if (!result_lo) {
                                auto vir = function->newVirRegister();
                                result_inst->parent->insert(
                                    std::next(result_inst->node),
                                    std::make_unique<mips::MoveInst>(vir, mips::PhyRegister::LO));
                                result_lo = vir;
                            }
                            lo->swapUseTo(result_lo);
                        }
                        result = std::make_tuple(result_inst, result_hi, result_lo);
                    } else {
                        map[identity] = std::make_tuple(mInst, hi, lo);
                    }
                }
            }
        }
    }

    void blockInline(mips::rFunction function) {
        std::list<mips::pBlock> newBlocks;
        std::unordered_map<mips::rLabel, mips::rLabel> labels;
        dbg(*function);
        for (auto &&block: *function) {
            auto newBlock = block->clone();
            labels[block->label.get()] = newBlock->label.get();
            while (true) {
                auto &&inst = newBlock->backInst();
                auto lbl = inst->getJumpLabel();
                if (!lbl || !std::holds_alternative<mips::rBlock>(lbl->parent)) break;
                auto suc = std::get<mips::rBlock>(lbl->parent);
                if (suc == function->exitB.get()) break;
                if (suc == block.get()) break;
                newBlock->mergeBlock(suc);
            }
            dbg(*newBlock, newBlock->subBlocks.size());
            auto node = newBlocks.insert(newBlocks.cend(), std::move(newBlock));
            newBlocks.back()->node = node;
        }
        for (auto &&block: newBlocks) {
            for (auto &&sub: block->subBlocks) {
                auto &&inst = sub->back();
                if (!labels.count(inst->getJumpLabel())) continue;
                inst->setJumpLabel(labels[inst->getJumpLabel()]);
            }
        }
        function->blocks.swap(newBlocks);
        clearDeadBlock(function);
        dbg(*function);
        // original blocks are deleted automatically
    }

    void exitBBInline(mips::rFunction function) {

    }
}
