//
// Created by toby on 2024/8/6.
//

#include "riscv/instruction.h"
#include <iterator>
#include <memory>
#include "backend/operand.h"
#include "riscv/alias.h"
#include "riscv/operand.h"
#include "util.h"

namespace backend::riscv {

namespace {
bool isLegalImm(rImmediate imm, int low, int high) {
    if (auto i = dynamic_cast<rIntImmediate>(imm)) {
        return low <= i->value && i->value <= high;
    } else if (auto s = dynamic_cast<rSplitImmediate>(imm)) {
        return true;
    } else if (auto j = dynamic_cast<rJoinImmediate>(imm)) {
        auto value = j->accumulate();
        return j->label == nullptr && low <= value && value <= high;
    } else {
        __builtin_unreachable();
    }
}

// return {hi[31:12], lo[11:0]}
std::pair<pImmediate, pImmediate> splitImm(rImmediate imm) {
    std::pair<pImmediate, pImmediate> ret;
    auto &[hi, lo] = ret;
    if (auto i = dynamic_cast<rIntImmediate>(imm)) {
        auto u = static_cast<unsigned>(i->value);
        auto hiv = u >> 12, lov = u & 0xFFF;
        if (lov & 0x800) hiv = (hiv + 1) & 0xFFFFF, lov |= 0xFFFF'F000;
        hi = create_imm((int)hiv);
        lo = create_imm((int)lov);
    } else if (auto s = dynamic_cast<rSplitImmediate>(imm)) {
        TODO("should not split twice");
    } else {
        hi = create_imm_hi(imm->clone());
        lo = create_imm_lo(imm->clone());
    }
    return ret;
}

// split imm to x31 and push to parent
template <typename Func>
void splitAndPush(rImmediate imm, Func &&push) {
    auto [hi, lo] = splitImm(imm);
    push(std::make_unique<UInstruction>(Instruction::Ty::LUI, "x31"_R, std::move(hi)));
    if (auto i = dynamic_cast<rIntImmediate>(lo.get()); i && i->value == 0) return;
    push(std::make_unique<IInstruction>(Instruction::Ty::ADDI, "x31"_R, "x31"_R, std::move(lo)));
}

// split imm+reg to x31+lo and push to parent
template <typename Func>
pImmediate splitAndPush(rImmediate imm, rRegister reg, Func &&push) {
    auto [hi, lo] = splitImm(imm);
    push(std::make_unique<UInstruction>(Instruction::Ty::LUI, "x31"_R, std::move(hi)));
    if (reg == "x0"_R) return std::move(lo);
    push(std::make_unique<RInstruction>(Instruction::Ty::ADD, "x31"_R, "x31"_R, reg));
    return std::move(lo);
}

Instruction::Ty TyI2R(Instruction::Ty ty) {
    switch (ty) {
    case Instruction::Ty::ADDI: return Instruction::Ty::ADD;
    case Instruction::Ty::SLTI: return Instruction::Ty::SLT;
    case Instruction::Ty::SLTIU: return Instruction::Ty::SLTU;
    case Instruction::Ty::XORI: return Instruction::Ty::XOR;
    case Instruction::Ty::ORI: return Instruction::Ty::OR;
    case Instruction::Ty::ANDI: return Instruction::Ty::AND;
    case Instruction::Ty::ADDIW: return Instruction::Ty::ADDW;
    case Instruction::Ty::SLLI:
    case Instruction::Ty::SRLI:
    case Instruction::Ty::SRAI:
    case Instruction::Ty::SLLIW:
    case Instruction::Ty::SRLIW:
    case Instruction::Ty::SRAIW: TODO("should not overflow!!!");
    default: TODO("why you reach here??");
    }
}
}  // namespace

inst_node_t IInstruction::legalize() {
    if (isLegalImm(imm.get(), -2048, 2047)) return std::next(node);
    const auto push = [this](pInstruction inst) { parent->insert(node, std::move(inst)); };
    if (isLoad()) {
        auto offset = splitAndPush(imm.get(), rs1(), push);
        push(std::make_unique<IInstruction>(ty, rd(), "x31"_R, std::move(offset)));
    } else {
        splitAndPush(imm.get(), push);
        auto new_ty = TyI2R(ty);
        push(std::make_unique<RInstruction>(new_ty, rd(), rs1(), "x31"_R));
    }
    return parent->erase(node);
}

inst_node_t SInstruction::legalize() {
    if (isLegalImm(imm.get(), -2048, 2047)) return std::next(node);
    const auto push = [this](pInstruction inst) { parent->insert(node, std::move(inst)); };
    auto offset = splitAndPush(imm.get(), rs2(), push);
    push(std::make_unique<SInstruction>(ty, rs1(), "x31"_R, std::move(offset)));
    return parent->erase(node);
}

inst_node_t UInstruction::legalize() {
    if (isLegalImm(imm.get(), 0, (1 << 20) - 1)) return std::next(node);
    TODO("Illegal immediate for UInstruction!!!");
}

}  // namespace backend::riscv
