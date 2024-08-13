//
// Created by toby on 2024/8/5.
//

#include "riscv/opt.h"
#include <memory>
#include <unordered_map>
#include "riscv/instruction.h"
#include "riscv/operand.h"
#include "riscv/reg_alloca.h"

namespace backend::riscv {
void clearDeadCode(rFunction function) {
    bool changed = true;
    while (changed) {
        changed = false;
        compute_blocks_info(function);
        for (auto &block : all_sub_blocks(function)) {
            std::vector<rInstruction> dead = {};
            auto used = block->liveOut;
            auto pred = [&](auto &&reg) -> bool {
                static const auto zero = "x0"_R;
                return reg != zero && used.count(reg);
            };
            auto addUsed = [&](auto &&inst) {
                std::for_each(inst->regUse.begin(), inst->regUse.end(),
                              [&](auto &&reg) { used.insert(reg); });
            };
            auto earseDef = [&](auto &&inst) {
                std::for_each(inst->regDef.begin(), inst->regDef.end(),
                              [&](auto &&reg) { used.erase(reg); });
            };
            for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
                auto inst = dynamic_cast<Instruction *>(it->get());
                assert(inst != nullptr);
                if (inst->isJumpBranch() || inst->isStore()) {
                    earseDef(inst);
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
                dead.push_back(inst);
                changed = true;
            }
            for (auto &inst : dead) block->erase(inst->node);
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
    for (auto &block : *function) unordered.insert(block.get());
    for (auto &block : *function) unordered.erase(next(block));
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

namespace {
template <typename K, typename V>
using kmap = std::unordered_map<K, V>;
using Ty = Instruction::Ty;

std::pair<uint64_t, int> div2mul(uint32_t divisor) {
    std::pair<uint64_t, int> result = {0, -1};
    for (int l = 0; l < 32; l++) {
        constexpr int N = 32;
        auto p1 = uint64_t{1} << (N + l);
        auto m = (p1 + (uint64_t{1} << l)) / divisor;
        if (m * divisor <= p1) continue;
        if (result.second == -1 || m < result.first) result = {m, l};
    }
    return result;
}

struct Node {
    rRegister reg;
    pInstruction inst;
    std::vector<Node *> suc;
    uint32_t degree = 0;

    explicit Node(rRegister reg) noexcept : reg{reg} {}
};

template <bool X32>
void process_mul_impl(rFunction func, kmap<uint64_t, std::unique_ptr<Node>> &map, uint32_t mul) {
    assert(mul > 0);
    constexpr Ty SLLI = X32 ? Ty::SLLIW : Ty::SLLI;
    constexpr Ty ADD = X32 ? Ty::ADDW : Ty::ADD;
    constexpr Ty SUB = X32 ? Ty::SUBW : Ty::SUB;
    if (map.count(mul)) return;
    auto &node_ = map[mul] = std::make_unique<Node>(func->newVirRegister());
    auto node = node_.get();
    if (mul % 2 == 0) {
        int zcnt = __builtin_ctz(mul);
        process_mul_impl<X32>(func, map, mul >> zcnt);
        auto &dep = map[mul >> zcnt];
        node->inst = std::make_unique<IInstruction>(SLLI, node->reg, dep->reg, create_imm(zcnt));
        dep->suc.push_back(node);
        node->degree = 1;
    } else if (mul == -1) {
        auto &dep = map[1];
        node->inst = std::make_unique<RInstruction>(SUB, node->reg, "x0"_R, dep->reg);
        dep->suc.push_back(node);
        node->degree = 1;
    } else if ((mul & 7) == 7) {
        int ocnt = __builtin_ctz(mul + 1);
        process_mul_impl<X32>(func, map, mul + 1);
        auto &dep = map[mul + 1], &ori = map[1];
        node->inst = std::make_unique<RInstruction>(SUB, node->reg, dep->reg, ori->reg);
        dep->suc.push_back(node);
        ori->suc.push_back(node);
        node->degree = 2;
    } else if (mul == 3) {
        process_mul_impl<X32>(func, map, 2);
        auto &d1 = map[1], &d2 = map[2];
        node->inst = std::make_unique<RInstruction>(ADD, node->reg, d1->reg, d2->reg);
        d1->suc.push_back(node);
        d2->suc.push_back(node);
        node->degree = 2;
    } else if ((mul & 3) == 3) {
        process_mul_impl<X32>(func, map, mul - 3);
        process_mul_impl<X32>(func, map, 3);
        auto &dep = map[mul - 3], &ori = map[3];
        node->inst = std::make_unique<RInstruction>(ADD, node->reg, dep->reg, ori->reg);
        dep->suc.push_back(node);
        ori->suc.push_back(node);
        node->degree = 2;
    } else if ((mul & 1) == 1) {
        process_mul_impl<X32>(func, map, mul - 1);
        auto &dep = map[mul - 1], &ori = map[1];
        node->inst = std::make_unique<RInstruction>(ADD, node->reg, dep->reg, ori->reg);
        dep->suc.push_back(node);
        ori->suc.push_back(node);
        node->degree = 2;
    } else {
        __builtin_unreachable();
    }
}
}  // namespace

rRegister process_mul(rBlock block, Instruction::Ty ty, rRegister reg, uint64_t mul) {
    if (mul == 0) return "x0"_R;
    if (mul == 1) return reg;
    auto func = block->parent;
    kmap<uint64_t, std::unique_ptr<Node>> map;
    map[1] = std::make_unique<Node>(reg);
    if (ty == Ty::MUL)
        process_mul_impl<false>(func, map, mul);
    else if (ty == Ty::MULW)
        process_mul_impl<true>(func, map, mul);
    else
        __builtin_unreachable();
    if (map.size() > 5) {
        auto dst = func->newVirRegister();
        block->push_back(std::make_unique<LiInstruction>(dst, mul));
        block->push_back(std::make_unique<RInstruction>(ty, dst, reg, dst));
        return dst;
    } else {
        auto dfs = [&map, &block](Node *node, auto &&self) -> void {
            if (node->inst != nullptr) block->push_back(std::move(node->inst));
            for (auto &x : node->suc)
                if (--x->degree == 0) self(x, self);
        };
        dfs(map[1].get(), dfs);
        return map[mul]->reg;
    }
}

rRegister process_div(rBlock block, rRegister reg, int32_t div) {
    assert(div != 0);
    if (div == 1) return reg;
    auto func = block->parent;
    if (div == -1) {
        auto dst = func->newVirRegister();
        block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, "x0"_R, reg));
        return dst;
    }
    if (__builtin_popcount(std::abs(div)) == 1) {
        int zcnt = __builtin_ctz(std::abs(div));
        auto temp = func->newVirRegister(), dst = func->newVirRegister();
        block->push_back(std::make_unique<IInstruction>(Ty::SRAIW, temp, reg, create_imm(31)));
        block->push_back(
            std::make_unique<IInstruction>(Ty::ANDI, temp, temp, create_imm((1 << zcnt) - 1)));
        block->push_back(std::make_unique<RInstruction>(Ty::ADDW, dst, temp, reg));
        block->push_back(std::make_unique<IInstruction>(Ty::SRAIW, dst, dst, create_imm(zcnt)));
        if (div < 0) block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, "x0"_R, dst));
        return dst;
    }
    auto [m, l] = div2mul(std::abs(div));
    if (l == -1) return nullptr;
    auto dst = func->newVirRegister();
    auto mx = process_mul(block, Ty::MUL, reg, m);
    block->push_back(std::make_unique<IInstruction>(Ty::SRAI, mx, mx, create_imm(32 + l)));
    block->push_back(std::make_unique<IInstruction>(Ty::SRAIW, dst, reg, create_imm(31)));
    if (div < 0)
        block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, dst, mx));
    else
        block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, mx, dst));
    return dst;
}

rRegister process_divu(rBlock block, rRegister reg, uint32_t div) {
    assert(div != 0);
    if (div == 1) return reg;
    auto func = block->parent;
    if (__builtin_popcount(div) == 1) {
        int zcnt = __builtin_clz(div);
        auto dst = func->newVirRegister();
        block->push_back(std::make_unique<IInstruction>(Ty::SRLIW, dst, reg, create_imm(zcnt)));
        return dst;
    }
    auto [m, l] = div2mul(div);
    if (l == -1) return nullptr;
    auto zr = func->newVirRegister(), dst = func->newVirRegister();
    block->push_back(std::make_unique<FpConvInstruction>(Ty::ZEXT_W, zr, reg));
    auto mx = process_mul(block, Ty::MUL, zr, m);
    block->push_back(std::make_unique<IInstruction>(Ty::SRLI, dst, mx, create_imm(32 + l)));
    return dst;
}

rRegister process_rem(rBlock block, rRegister reg, int32_t div) {
    assert(div != 0);
    div = std::abs(div);
    if (div == 1) return "x0"_R;
    auto func = block->parent;
    if (__builtin_popcount(div) == 1) {
        int zcnt = __builtin_ctz(div);
        auto temp = func->newVirRegister(), dst = func->newVirRegister();
        block->push_back(std::make_unique<IInstruction>(Ty::SRAIW, temp, reg, create_imm(31)));
        block->push_back(std::make_unique<IInstruction>(Ty::ANDI, temp, temp, create_imm(div - 1)));
        block->push_back(std::make_unique<RInstruction>(Ty::ADDW, dst, temp, reg));
        block->push_back(std::make_unique<IInstruction>(Ty::ANDI, dst, dst, create_imm(div - 1)));
        block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, dst, temp));
        return dst;
    }
    auto d = process_div(block, reg, div);
    if (d == nullptr) return nullptr;
    auto mx = process_mul(block, Ty::MULW, d, div);
    auto dst = func->newVirRegister();
    block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, reg, mx));
    return dst;
}

rRegister process_remu(rBlock block, rRegister reg, uint32_t div) {
    assert(div != 0);
    if (div == 1) return "x0"_R;
    auto func = block->parent;
    if (__builtin_popcount(div) == 1) {
        auto dst = func->newVirRegister();
        block->push_back(std::make_unique<IInstruction>(Ty::ANDI, dst, reg,
                                                        create_imm(static_cast<int>(div - 1))));
        return dst;
    }
    auto d = process_divu(block, reg, div);
    if (d == nullptr) return nullptr;
    auto mx = process_mul(block, Ty::MULW, d, div);
    auto dst = func->newVirRegister();
    block->push_back(std::make_unique<RInstruction>(Ty::SUBW, dst, reg, mx));
    return dst;
}

}  // namespace backend::riscv
