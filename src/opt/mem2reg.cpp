//
// Created by toby on 2023/11/14.
//

#include <queue>
#include "opt.h"

namespace mir {
    void reCalcBBInfo(Function *func) {
        func->calcPreSuc();
        calcDominators(func);
        calcDF(func);
    }

    void calcDominators(Function *func) {
        std::set<BasicBlock *> all{func->bbs.begin(), func->bbs.end()};
        all.insert(func->exitBB.get());
        const auto calc_dom = [&all](auto bb) {
            auto dominators = all;
            std::set<BasicBlock *> temp;
            for (auto pre: bb->predecessors) {
                if (pre->dominators.count(bb)) continue;
                std::set_intersection(dominators.begin(), dominators.end(),
                                      pre->dominators.begin(), pre->dominators.end(),
                                      std::inserter(temp, temp.cend()));
                dominators = std::move(temp);
                temp = {};
            }
            if (dominators.size() == all.size())
                return bb->dominators = {bb}, void();
            bb->dominators = std::move(dominators);
            bb->dominators.insert(bb);
        };

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto bb: func->bbs) {
                auto pre_size = bb->dominators.size();
                calc_dom(bb);
                if (bb->dominators.size() != pre_size)
                    changed = true;
            }
        }
        calc_dom(func->exitBB.get());

        constexpr auto calc_idom = [](auto bb) {
            bb->idom = nullptr;
            for (auto dom: bb->dominators)
                if (bb != dom && (!bb->idom || dom->dominators.count(bb->idom)))
                    bb->idom = dom;
        };
        for (auto bb: func->bbs) calc_idom(bb);
        calc_idom(func->exitBB.get());
    }

    void calcDF(const Function *func) {
        constexpr auto calc = [](auto x, auto y) {
            while (x != y->idom) {
                x->df.insert(y);
                x = x->idom;
            }
        };

        std::unordered_set<BasicBlock *> vis{};
        auto dfs = [&vis](BasicBlock *bb, auto &&f, auto &&self) -> void {
            vis.insert(bb);
            for (auto suc: bb->successors) {
                f(bb, suc);
                if (vis.count(suc)) continue;
                self(suc, f, self);
            }
        };

        dfs(func->bbs.front(), calc, dfs);
    }

    static void calcPhi(const Function *func, const Instruction::alloca_ *alloc) {
        assert(alloc->getType() == Type::getI32Type());
        std::unordered_map<BasicBlock *, Value *> liveInV; // live in value
        std::unordered_map<BasicBlock *, Value *> defs; // def value (live out value)

        // Step 1. calc defs
        std::queue<BasicBlock *> W; // store queue
        for (auto bb: func->bbs) {
            for (auto it = bb->instructions.rbegin(); it != bb->instructions.rend(); ++it) {
                if (auto store = dynamic_cast<Instruction::store *>(*it);
                    store && store->getDest() == alloc) {
                    defs[bb] = store->getSrc();
                    W.push(bb);
                    break;
                }
            }
        }

        // Step 2. mark phi
        std::unordered_set<BasicBlock *> F; // should add phi
        while (!W.empty()) {
            auto bb = W.front();
            W.pop();
            for (auto df: bb->df) {
                if (F.count(df)) continue;
                F.insert(df);
                auto phi = new Instruction::phi(Type::getI32Type());
                liveInV[df] = phi;
                df->push_front(phi);
                if (!defs.count(df))
                    W.push(df);
            }
        }

        // Step 3. create phi
        // NOLINTNEXTLINE
        const auto find_d = [&liveInV, &defs](BasicBlock *bb, auto &&self) -> Value *{
            if (defs.count(bb)) return defs[bb];
            if (liveInV.count(bb)) return defs[bb] = liveInV[bb];
            if (bb->idom == nullptr) return defs[bb] = getIntegerLiteral(0);
            return defs[bb] = self(bb->idom, self);
        };
        for (auto bb: F) {
            auto phi = dynamic_cast<Instruction::phi *>(liveInV[bb]);
            assert(phi);
            for (auto pre: bb->predecessors)
                phi->addIncomingValue({find_d(pre, find_d), pre});
        }

        // Step 4. convert load & store
        // NOLINTNEXTLINE
        const auto find_v = [&liveInV, &defs](BasicBlock *bb, auto &&self) -> Value *{
            if (liveInV.count(bb)) return liveInV[bb];
            if (bb->idom == nullptr) return liveInV[bb] = getIntegerLiteral(0);
            if (defs.count(bb->idom)) return liveInV[bb] = defs[bb->idom];
            return liveInV[bb] = self(bb->idom, self);
        };
        for (auto bb: func->bbs) {
            Value *last_store = find_v(bb, find_v);
            auto it = bb->instructions.begin();
            while (it != bb->instructions.end()) {
                auto &inst = *it;
                if (auto store = dynamic_cast<Instruction::store *>(inst);
                    store && store->getDest() == alloc) {
                    last_store = store->getSrc();
                    it = bb->erase(store);
                } else if (auto load = dynamic_cast<Instruction::load *>(inst);
                    load && load->getPointerOperand() == alloc) {
                    assert(last_store);
                    load->moveTo(last_store);
                    it = bb->erase(load);
                } else ++it;
            }
        }
    }

    void calcPhi(Function *func) {
        reCalcBBInfo(func);
        for (auto inst: func->bbs.front()->instructions) {
            if (auto alloc = dynamic_cast<Instruction::alloca_ *>(inst)) {
                if (alloc->getType() != Type::getI32Type()) continue;
                calcPhi(func, alloc);
            } else break;
        }
    }

    void clearDeadInst(const Function *func) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto bb: func->bbs) {
                auto it = bb->instructions.begin();
                while (it != bb->instructions.end()) {
                    if (auto inst = *it;
                        inst->isUsed() || !inst->isValue() || inst->isTerminator() || inst->isCall())
                        ++it;
                    else it = bb->erase(inst), changed = true;
                }
            }
        }
    }

    void clearDeadBlock(Function *func) {
        bool changed = true;
        while (changed) {
            changed = false;
            func->calcPreSuc();
            auto it = ++func->bbs.begin();
            while (it != func->bbs.end()) {
                if (auto bb = *it; bb->isUsed()) {
                    ++it;
                } else {
                    delete bb;
                    changed = true;
                    it = func->bbs.erase(it);
                }
            }
        }
    }

    void mergeEmptyBlock(Function *func) {
        // First block shouldn't be merged
        auto it = ++func->bbs.begin();
        while (it != func->bbs.end()) {
            auto bb = *it;
            if (bb->instructions.size() > 1) {
                ++it;
                continue;
            }
            auto inst = bb->instructions.front();
            auto br = dynamic_cast<Instruction::br *>(inst);
            if (!br || br->hasCondition()) {
                ++it;
                continue;
            }
            // this block only contains a 'br %label' instruction
            bb->moveTo(br->getTarget());
            it = func->bbs.erase(it);
            delete bb;
        }
        // after block merging, some br instructions may become:
        // br i1 %cond, %same_label, %same_label
        for (auto &bb: func->bbs) {
            auto inst = bb->instructions.back();
            auto br = dynamic_cast<Instruction::br *>(inst);
            if (!br || !br->hasCondition()) continue;
            if (br->getIfTrue() != br->getIfFalse()) continue;
            auto label = br->getIfTrue();
            substitude(br, new Instruction::br(label));
        }
    }
}
