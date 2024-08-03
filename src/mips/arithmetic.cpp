//
// Created by toby on 2023/12/8.
//

#include "mips/backend_opt.h"
#include "mips/reg_alloca.h"

#define CASE(ty) case Instruction::Ty::ty
#define CASE_DO(ty) \
    case Instruction::Ty::ty: return do##ty()

namespace backend::mips {
inline const auto zero = PhyRegister::get(0);

inline inst_node_t substitute(rInstructionBase _old, pInstructionBase _new) {
    _new->node = _old->node;
    _new->parent = _old->parent;
    _new.swap(*_old->node);
    // '_new' free here (in fact, it's _old that is freed)
    return std::next(_new->node);
}

inline inst_node_t substitute(rInstruction _inst, rRegister _old, rRegister _new) {
    if (_old->defUsers.size() == 1) return _old->swapUseTo(_new), _inst->parent->erase(_inst->node);
    return substitute(_inst, std::make_unique<MoveInst>(_old, _new));
}

inst_node_t arithmeticFolding(rBinaryRInst self) {
    // ADDU, SUBU, AND, OR, NOR, XOR, SLLV, SRAV, SRLV, SLT, SLTU, MOVN, MOVZ, CLO, CLZ
    auto &&dst = self->dst();
    auto &&src1 = self->src1();
    auto &&src2 = self->src2();
    auto block = self->parent;
    if (dst == zero) return block->erase(self->node);

    auto doADDU = [&]() {
        if (src1 == zero) return substitute(self, dst, src2);
        if (src2 == zero) return substitute(self, dst, src1);
        return std::next(self->node);
    };
    auto doSUBU = [&]() {
        if (src2 == zero) return substitute(self, dst, src1);
        if (src1 == src2) return substitute(self, dst, zero);
        return std::next(self->node);
    };
    auto doAND = [&]() {
        if (src1 == zero || src2 == zero) return substitute(self, dst, zero);
        if (src1 == src2) return substitute(self, dst, src1);
        return std::next(self->node);
    };
    auto doOR = [&]() {
        if (src1 == zero) return substitute(self, dst, src2);
        if (src2 == zero) return substitute(self, dst, src1);
        if (src1 == src2) return substitute(self, dst, src1);
        return std::next(self->node);
    };
    auto doXOR = [&]() {
        if (src1 == zero) return substitute(self, dst, src2);
        if (src2 == zero) return substitute(self, dst, src1);
        if (src1 == src2) return substitute(self, dst, zero);
        return std::next(self->node);
    };
    auto doSLLV = [&]() {
        if (src1 == zero) return substitute(self, dst, zero);
        if (src2 == zero) return substitute(self, dst, src1);
        return std::next(self->node);
    };
    auto doSRAV = doSLLV;
    auto doSRLV = doSLLV;
    auto doSLT = [&]() {
        if (src1 == src2) return substitute(self, dst, zero);
        return std::next(self->node);
    };
    auto doSLTU = [&]() {
        if (src1 == src2) return substitute(self, dst, zero);
        if (src2 == zero) return substitute(self, dst, zero);
        return std::next(self->node);
    };
    auto doMOVN = [&]() {
        if (src2 == zero) return block->erase(self->node);
        return std::next(self->node);
    };
    auto doMOVZ = [&]() {
        if (src2 == zero) return substitute(self, dst, src1);
        return std::next(self->node);
    };

    switch (self->ty) {
        CASE_DO(ADDU);
        CASE_DO(SUBU);
        CASE_DO(AND);
        CASE_DO(OR);
        CASE_DO(XOR);
        CASE_DO(SLLV);
        CASE_DO(SRAV);
        CASE_DO(SRLV);
        CASE_DO(SLT);
        CASE_DO(SLTU);
        CASE_DO(MOVN);
        CASE_DO(MOVZ);
    // TODO: CASE_DO(CLO);
    // TODO: CASE_DO(CLZ);
    default: return std::next(self->node);
    }
}

inst_node_t arithmeticFolding(rBinaryMInst self) {
    auto [hi, lo] = find_hi_lo(self);
    auto &&src1 = self->src1();
    auto &&src2 = self->src2();
    auto block = self->parent;
    if (src1 == zero || src2 == zero) {
        if (hi) hi->swapUseTo(zero);
        if (lo) lo->swapUseTo(zero);
        return block->erase(self->node);
    }
    if (self->ty != Instruction::Ty::MUL && self->ty != Instruction::Ty::MULTU)
        return std::next(self->node);
    auto imm = getLastImm(self, src2);
    if (!imm || __builtin_popcount(*imm) != 1) return std::next(self->node);
    if (*imm == 1) {
        if (hi) hi->swapUseTo(zero);
        if (lo) lo->swapUseTo(src1);
        return block->erase(self->node);
    }
    if (lo) {
        auto dst = block->parent->parent->newVirRegister();
        block->insert(self->node, std::make_unique<BinaryIInst>(Instruction::Ty::SLL, dst, src1,
                                                                __builtin_ctz(*imm)));
        lo->swapUseTo(dst);
    }
    if (hi) {
        auto dst = block->parent->parent->newVirRegister();
        block->insert(self->node, std::make_unique<BinaryIInst>(Instruction::Ty::SRL, dst, src1,
                                                                32 - __builtin_ctz(*imm)));
        hi->swapUseTo(dst);
    }
    return block->erase(self->node);
}

void arithmeticFolding(rFunction function) {
    for (auto &&block : all_sub_blocks(function)) {
        for (auto it = block->begin(); it != block->end();) {
            auto inst = it->get();
            if (auto inst_r = dynamic_cast<rBinaryRInst>(inst))
                it = arithmeticFolding(inst_r);
            else if (auto inst_m = dynamic_cast<rBinaryMInst>(inst))
                it = arithmeticFolding(inst_m);
            else {
                ++it;
            }
        }
    }
}
}  // namespace backend::mips
