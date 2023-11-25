//
// Created by toby on 2023/11/24.
// Constant Folding
//

#include "opt.h"

// extern mir::Manager mir_manager;

namespace mir {
    static inst_node_t constantFolding(Instruction::br *br) {
        if (!br->hasCondition()) return br->node;
        BasicBlock *target;
        if (br->getIfFalse() == br->getIfTrue())
            target = br->getIfTrue();
        else if (auto cond = dynamic_cast<BooleanLiteral *>(br->getCondition()))
            target = cond->value ? br->getIfTrue() : br->getIfFalse();
        else return br->node;
        return substitute(br, new Instruction::br(target));
    }

    template<Instruction::InstrTy ty>
    static inst_node_t constantFolding(Instruction::_binary_instruction<ty> *binary) {
        auto lhs = dynamic_cast<IntegerLiteral *>(binary->getLhs());
        auto rhs = dynamic_cast<IntegerLiteral *>(binary->getRhs());
        if constexpr (ty == Instruction::ADD || ty == Instruction::MUL
                      || ty == Instruction::AND || ty == Instruction::OR || ty == Instruction::XOR) {
            if (lhs && !rhs) {
                using T = std::remove_pointer_t<decltype(binary)>;
                return substitute(binary, new T(binary->getRhs(), lhs));
            }
        }
        if (!lhs || !rhs) return binary->node;
        return substitute(binary, binary->calc());
    }

    static inst_node_t constantFolding(Instruction::load *load) {
        constexpr auto calc = [](Value *ptr, auto &&self) -> Literal *const *{
            if (auto var = dynamic_cast<GlobalVar *>(ptr))
                return var->isConst() && var->init ? &var->init : nullptr;
            if (auto gep = dynamic_cast<Instruction::getelementptr *>(ptr)) {
                auto var = self(gep->getPointerOperand(), self);
                if (!var) return nullptr;
                for (auto i = 0; i < gep->getNumIndices(); ++i) {
                    auto index = dynamic_cast<IntegerLiteral *>(gep->getIndexOperand(i));
                    if (!index) return nullptr;
                    if (i) {
                        auto vec = dynamic_cast<ArrayLiteral *>(*var);
                        assert(vec);
                        var = &vec->values[0];
                    }
                    var += index->value;
                }
                return var;
            }
            return nullptr;
        };
        auto lit = calc(load->getPointerOperand(), calc);
        if (!lit) return load->node;
        return substitute(load, *lit);
    }

    template<Instruction::InstrTy ty>
    static inst_node_t constantFolding(Instruction::_conversion_instruction<ty> *conversion) {
        auto literal = dynamic_cast<Literal *>(conversion->getValueOperand());
        if (!literal) return conversion->node;
        auto i32 = dynamic_cast<IntegerLiteral *>(literal);
        auto i1 = dynamic_cast<BooleanLiteral *>(literal);
        if constexpr (ty == Instruction::TRUNC) // assmue i32 -> i1
            return substitute(conversion, getBooleanLiteral(i32->value & 1));
        if constexpr (ty == Instruction::ZEXT) // assmue i1 -> i32
            return substitute(conversion, getIntegerLiteral(i1->value ? 1 : 0));
        if constexpr (ty == Instruction::SEXT) // assmue i1 -> i32
            return substitute(conversion, getIntegerLiteral(i1->value ? -1 : 0));
        __builtin_unreachable();
    }

    static inst_node_t constantFolding(Instruction::icmp *icmp) {
        if (icmp->getLhs() == icmp->getRhs()) {
            if (icmp->cond == Instruction::icmp::EQ)
                return substitute(icmp, getBooleanLiteral(true));
            if (icmp->cond == Instruction::icmp::NE)
                return substitute(icmp, getBooleanLiteral(false));
        }
        auto lhs = dynamic_cast<IntegerLiteral *>(icmp->getLhs());
        auto rhs = dynamic_cast<IntegerLiteral *>(icmp->getRhs());
        if (!lhs || !rhs) return icmp->node;
        auto result = [&] {
#define CASE(C) case Instruction::icmp::C
            auto &&lhsv = lhs->value, &&rhsv = rhs->value;
            auto &&lhsu = static_cast<unsigned>(lhsv), &&rhsu = static_cast<unsigned>(rhsv);
            switch (icmp->cond) {
                //EQ, NE, UGT, UGE, ULT, ULE, SGT, SGE, SLT, SLE
                CASE(EQ): return lhsv == rhsv;
                CASE(NE): return lhsv != rhsv;
                CASE(UGT): return lhsu > rhsu;
                CASE(UGE): return lhsu >= rhsu;
                CASE(ULT): return lhsu < rhsu;
                CASE(ULE): return lhsu <= rhsu;
                CASE(SGT): return lhsv > rhsv;
                CASE(SGE): return lhsv >= rhsv;
                CASE(SLT): return lhsv < rhsv;
                CASE(SLE): return lhsv <= rhsv;
            }
            __builtin_unreachable();
#undef CASE
        }();
        return substitute(icmp, getBooleanLiteral(result));
    }

    static inst_node_t constantFolding(Instruction::phi *phi) {
        phi->parent->parent->calcPreSuc();
        for (auto i = 0; i < phi->getNumIncomingValues();)
            if (auto bb = phi->getIncomingValue(i).second;
                    !phi->parent->predecessors.count(bb))
                phi->eraseIncomingValue(bb);
            else ++i;
        if (phi->getNumIncomingValues() == 0) return phi->node;
        auto [value, bb] = phi->getIncomingValue(0);
        for (auto i = 1; i < phi->getNumIncomingValues(); i++)
            if (value != phi->getIncomingValue(i).first) return phi->node;
        return substitute(phi, value);
    }

    static inst_node_t constantFolding(Instruction::select *select) {
        Value *result;
        if (select->getFalseValue() == select->getTrueValue()) {
            result = select->getTrueValue();
        } else if (auto lit = dynamic_cast<BooleanLiteral *>(select->getCondition())) {
            result = lit->value ? select->getTrueValue() : select->getFalseValue();
        } else {
            return select->node;
        }
        return substitute(select, result);
    }

    inst_node_t constantFolding(Instruction *inst) {
#define CASE(ty, cast) case Instruction::ty: return constantFolding(dynamic_cast<Instruction::cast *>(inst))
        switch (inst->instrTy) {
            CASE(BR, br);
            CASE(ADD, add);
            CASE(SUB, sub);
            CASE(MUL, mul);
            CASE(UDIV, udiv);
            CASE(SDIV, sdiv);
            CASE(UREM, urem);
            CASE(SREM, srem);
            CASE(SHL, shl);
            CASE(LSHR, lshr);
            CASE(ASHR, ashr);
            CASE(AND, and_);
            CASE(OR, or_);
            CASE(XOR, xor_);
            CASE(LOAD, load);
            CASE(TRUNC, trunc);
            CASE(ZEXT, zext);
            CASE(SEXT, sext);
            CASE(ICMP, icmp);
            CASE(PHI, phi);
            CASE(SELECT, select);
            // ret, alloca, store, getelementptr, call
            default: return inst->node;
        }
#undef CASE
    }

    void constantFolding(const Function *func) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto &bb: func->bbs)
                for (auto it = bb->instructions.begin(); it != bb->instructions.end();) {
                    assert((*it)->parent == bb);
                    assert((*it)->node == it);
                    if (auto new_it = constantFolding(*it); new_it != it) {
                        changed = true;
                        it = new_it;
                    } else ++it;
                }
        }
    }
}
