//
// Created by toby on 2023/11/16.
//

#include <map>
#include <algorithm>
#include "reg_alloca.h"
#include "backend_opt.h"

namespace mips {
    using backend::all_sub_blocks;
    using backend::compute_blocks_info;

    void clearDeadCode(rFunction function) {
        bool changed = true;
        while (changed) {
            changed = false;
            compute_blocks_info(function);
            for (auto &block: all_sub_blocks(function)) {
                std::vector<rInstruction> dead = {};
                auto used = block->liveOut;
                auto pred = [&](auto &&reg) -> bool {
                    static const auto zero = PhyRegister::get(0);
                    return reg != zero && used.count(reg);
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
                        earseDef(inst);
                        addUsed(inst);
                        continue;
                    }
                    assert(!inst->regDef.empty());
                    if (inst->ty == Instruction::Ty::MUL && !pred(inst->regDef[0])) {
                        // change to MULTU
                        pInstruction newInst = std::make_unique<BinaryMInst>(
                            Instruction::Ty::MULTU, inst->regUse[0], inst->regUse[1]);
                        newInst->parent = inst->parent;
                        newInst->node = inst->node;
                        inst.swap(newInst);
                    }
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

    void relocateBlock(rFunction function) {
        constexpr auto next = [](auto &&block) -> rBlock {
            if (auto label = block->backInst()->getJumpLabel();
                label && std::holds_alternative<rBlock>(label->parent))
                return std::get<rBlock>(label->parent);
            return nullptr;
        };

        std::unordered_set<rBlock> unordered, ordered;
        for (auto &block: *function)
            unordered.insert(block.get());
        for (auto &block: *function)
            unordered.erase(next(block));
        ordered.insert(function->exitB.get());

        std::list<pBlock> result;
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

    void divisionFold(rFunction function) {
        enum FoldType { NONE, MULU, DIV, DIVU };
        constexpr auto deduce_fold_type = [](auto ty) {
            switch (ty) {
                case Instruction::Ty::MUL:
                case Instruction::Ty::MULTU: return MULU;
                case Instruction::Ty::DIV: return DIV;
                case Instruction::Ty::DIVU: return DIVU;
                default: return NONE;
            }
        };

        for (auto &&block: all_sub_blocks(function)) {
            using operand_t = std::variant<int, rRegister>;
            std::map<std::tuple<FoldType, operand_t, operand_t>,
                std::tuple<rInstruction, rRegister, rRegister>> map;
            for (auto &&inst: *block) {
                auto dt = deduce_fold_type(inst->ty);
                if (dt == NONE) continue;
                auto mInst = dynamic_cast<rBinaryMInst>(inst.get());
                assert(mInst);
                auto src1_imm = getLastImm(mInst, mInst->src1());
                auto src2_imm = getLastImm(mInst, mInst->src2());
                auto src1_op = src1_imm ? operand_t(*src1_imm) : mInst->src1();
                auto src2_op = src2_imm ? operand_t(*src2_imm) : mInst->src2();
                auto identity = std::make_tuple(dt, src1_op, src2_op);
                auto [hi, lo] = find_hi_lo(mInst);
                if (map.count(identity)) {
                    auto &&result = map[identity];
                    auto &&[result_inst, result_hi, result_lo] = result;
                    static_assert(std::is_lvalue_reference_v<decltype(result)>, "wtf?");
                    if (hi) {
                        if (!result_hi) {
                            auto vir = function->newVirRegister();
                            result_inst->parent->insert(std::next(result_inst->node),
                                                        std::make_unique<MoveInst>(vir, PhyRegister::HI));
                            result_hi = vir;
                        }
                        hi->swapUseTo(result_hi);
                    }
                    if (lo) {
                        if (!result_lo) {
                            auto vir = function->newVirRegister();
                            result_inst->parent->insert(std::next(result_inst->node),
                                                        std::make_unique<MoveInst>(vir, PhyRegister::LO));
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
        clearDeadCode(function);
    }

    static std::pair<unsigned, int> div2mul(unsigned divisor) {
        for (int l = 0; l < 32; l++) {
            constexpr int N = 32;
            auto p1 = 1LLU << (N + l);
            auto m = (p1 + (1LLU << l)) / divisor;
            if (m > UINT32_MAX) continue;
            if (m * divisor < p1) continue;
            return {static_cast<unsigned>(m), l};
        }
        return {};
    }

    void div2mul(rFunction function) {
        divisionFold(function);
        bool changed = false;

        auto process_udiv = [&](rBinaryMInst inst, unsigned imm) {
            auto [hi, lo] = find_hi_lo(inst);
            auto block = inst->parent;
            if (__builtin_popcount(imm) == 1) {
                if (lo) {
                    auto dst = function->newVirRegister();
                    block->insert(inst->node, std::make_unique<BinaryIInst>(
                                      Instruction::Ty::SRL, dst, inst->src1(), __builtin_ctz(imm)));
                    lo->swapUseTo(dst);
                }
                if (hi) {
                    auto dst = function->newVirRegister();
                    block->insert(inst->node, std::make_unique<BinaryIInst>(
                                      Instruction::Ty::ANDI, dst, inst->src1(), imm - 1));
                    hi->swapUseTo(dst);
                }
                return block->erase(inst->node);
            }

            auto [m, l] = div2mul(imm);
            if (m == 0) return inst->node;
            auto m_reg = function->newVirRegister();
            auto temp = function->newVirRegister();
            auto dst = temp;
            block->insert(inst->node, std::make_unique<BinaryIInst>(
                              Instruction::Ty::LI, m_reg, static_cast<int>(m)));
            block->insert(inst->node, std::make_unique<BinaryMInst>(
                              Instruction::Ty::MULTU, inst->src1(), m_reg));
            block->insert(inst->node, std::make_unique<MoveInst>(
                              temp, PhyRegister::HI));
            if (l != 0) {
                dst = function->newVirRegister();
                block->insert(inst->node, std::make_unique<BinaryIInst>(
                                         Instruction::Ty::SRL, dst, temp, l));
            }
            changed = true;
            if (lo) lo->swapUseTo(dst);
            if (hi) {
                auto imm_reg = function->newVirRegister();
                auto mul_reg = function->newVirRegister();
                block->insert(inst->node, std::make_unique<BinaryIInst>(
                                  Instruction::Ty::LI, imm_reg, static_cast<int>(imm)));
                block->insert(inst->node, std::make_unique<BinaryMInst>(
                                  Instruction::Ty::MUL, mul_reg, dst, imm_reg));
                auto hi_dst = function->newVirRegister();
                block->insert(inst->node, std::make_unique<BinaryRInst>(
                                  Instruction::Ty::SUBU, hi_dst, inst->src1(), mul_reg));
                hi->swapUseTo(hi_dst);
            }
            return block->erase(inst->node);
        };

        auto process_sdiv = [&](rBinaryMInst inst, int imm)  {
            if (auto [m, l] = div2mul(abs(imm)); m == 0) return inst->node;
            auto [hi, lo] = find_hi_lo(inst);
            auto src_block = inst->parent->parent;
            auto result_block = src_block->splitBlock(inst->node);
            auto result_label = result_block->label.get();
            auto &neg_block = function->blocks.emplace_back(new Block(function));
            neg_block->node = std::prev(function->blocks.end());
            src_block->push_back(std::make_unique<BranchInst>(
                Instruction::Ty::BLTZ, inst->src1(), neg_block->label.get()));
            auto merge_dest = function->newVirRegister();

            auto imm_reg_1 = function->newVirRegister();
            src_block->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::LI, imm_reg_1, abs(imm)));
            auto pos_divu = new BinaryMInst(Instruction::Ty::DIVU, inst->src1(), imm_reg_1);
            src_block->push_back(pBinaryMInst{pos_divu});
            auto pos_temp = function->newVirRegister(), pos_dest = pos_temp;
            src_block->push_back(std::make_unique<MoveInst>(pos_temp, PhyRegister::LO));
            if (imm < 0) {
                pos_dest = function->newVirRegister();
                src_block->push_back(std::make_unique<BinaryRInst>(
                    Instruction::Ty::SUBU, pos_dest, PhyRegister::get(0), pos_temp));
            }
            src_block->push_back(std::make_unique<MoveInst>(merge_dest, pos_dest));
            src_block->push_back(std::make_unique<JumpInst>(Instruction::Ty::J, result_label));
            process_udiv(pos_divu, abs(imm));

            auto neg_src = function->newVirRegister();
            neg_block->push_back(std::make_unique<BinaryRInst>(
                Instruction::Ty::SUBU, neg_src, PhyRegister::get(0), inst->src1()));
            auto imm_reg_2 = function->newVirRegister();
            neg_block->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::LI, imm_reg_2, abs(imm)));
            auto neg_divu = new BinaryMInst(Instruction::Ty::DIVU, neg_src, imm_reg_2);
            neg_block->push_back(pBinaryMInst{neg_divu});
            auto neg_temp = function->newVirRegister(), neg_dest = neg_temp;
            neg_block->push_back(std::make_unique<MoveInst>(neg_temp, PhyRegister::LO));
            if (imm > 0) {
                neg_dest = function->newVirRegister();
                neg_block->push_back(std::make_unique<BinaryRInst>(
                    Instruction::Ty::SUBU, neg_dest, PhyRegister::get(0), neg_temp));
            }
            neg_block->push_back(std::make_unique<MoveInst>(merge_dest, neg_dest));
            neg_block->push_back(std::make_unique<JumpInst>(Instruction::Ty::J, result_label));
            process_udiv(neg_divu, abs(imm));

            auto result_sub = result_block->frontBlock();
            if (lo) lo->swapUseTo(merge_dest);
            if (hi) {
                auto imm_reg = function->newVirRegister();
                auto mul_reg = function->newVirRegister();
                result_sub->insert(inst->node, std::make_unique<BinaryIInst>(
                                  Instruction::Ty::LI, imm_reg, imm));
                result_sub->insert(inst->node, std::make_unique<BinaryMInst>(
                                  Instruction::Ty::MUL, mul_reg, merge_dest, imm_reg));
                auto hi_dst = function->newVirRegister();
                result_sub->insert(inst->node, std::make_unique<BinaryRInst>(
                                  Instruction::Ty::SUBU, hi_dst, inst->src1(), mul_reg));
                hi->swapUseTo(hi_dst);
            }

            changed = true;
            return result_sub->erase(inst->node);
        };

        for (auto block_it = function->begin(); block_it != function->end(); ++block_it) {
            for (auto sub_it = (*block_it)->subBlocks.begin(); sub_it != (*block_it)->subBlocks.end(); ++sub_it) {
                auto inst_it = (*sub_it)->begin();
                auto do_after = [&]() {
                    sub_it = (*inst_it)->parent->node;
                    block_it = (*sub_it)->parent->node;
                    if (!changed) ++inst_it;
                };
                for (; inst_it != (*sub_it)->end(); do_after()) {
                    changed = false;
                    auto &&inst = *inst_it;
                    if (inst->ty != Instruction::Ty::DIVU && inst->ty != Instruction::Ty::DIV) continue;
                    auto divInst = dynamic_cast<rBinaryMInst>(inst.get());
                    assert(divInst);
                    auto imm = getLastImm(divInst, divInst->src2());
                    if (imm == std::nullopt) continue;
                    inst_it = divInst->ty == Instruction::Ty::DIVU
                                  ? process_udiv(divInst, *imm)
                                  : process_sdiv(divInst, *imm);
                }
            }
        }
    }

    void clearDuplicateInst(rFunction function) {
        for (auto &&block: all_sub_blocks(function)) {
            for (auto it = block->begin(); it != block->end();) {
                auto inst = it->get();
                if (inst->ty != Instruction::Ty::LI && inst->ty != Instruction::Ty::LUI) {
                    ++it;
                    continue;
                }
                auto loadImm = dynamic_cast<rBinaryIInst>(inst);
                assert(loadImm);
                auto other_imm = getLastImm(loadImm, loadImm->dst());
                if (!loadImm->imm->isDyn() && other_imm && *other_imm == loadImm->imm->value) it = block->erase(it);
                else ++it;
            }
        }
    }

    void arithmeticFolding(rFunction function) {

    }
}
