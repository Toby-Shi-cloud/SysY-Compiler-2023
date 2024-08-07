//
// Created by toby on 2023/11/14.
//

#include <queue>
#include <stack>
#include <unordered_map>
#include "mir.h"
#include "mir/derived_value.h"
#include "mir/type.h"
#include "opt/opt.h"
#include "settings.h"

namespace mir {
void Function::calcDominators() const {
    std::set<BasicBlock *> all{bbs.begin(), bbs.end()};
    all.insert(exitBB);
    const auto calc_dom = [&all](auto bb) {
        auto dominators = all;
        std::set<BasicBlock *> temp;
        for (auto pre : bb->predecessors) {
            if (pre->dominators.count(bb)) continue;
            std::set_intersection(dominators.begin(), dominators.end(), pre->dominators.begin(),
                                  pre->dominators.end(), std::inserter(temp, temp.cend()));
            dominators = std::move(temp);
            temp = {};
        }
        if (dominators.size() == all.size()) return bb->dominators = {bb}, void();
        bb->dominators = std::move(dominators);
        bb->dominators.insert(bb);
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto bb : bbs) {
            auto pre_size = bb->dominators.size();
            calc_dom(bb);
            if (bb->dominators.size() != pre_size) changed = true;
        }
    }
    calc_dom(exitBB);

    constexpr auto calc_idom = [](auto bb) {
        bb->idom = nullptr;
        for (auto dom : bb->dominators)
            if (bb != dom && (!bb->idom || dom->dominators.count(bb->idom))) bb->idom = dom;
    };
    for (auto bb : bbs) calc_idom(bb);
    calc_idom(exitBB);
}

void Function::calcDF() const {
    constexpr auto calc = [](auto x, auto y) {
        while (x != y->idom) {
            x->df.insert(y);
            x = x->idom;
        }
    };

    std::unordered_set<BasicBlock *> vis{};
    auto dfs = [&vis](BasicBlock *bb, auto &&f, auto &&self) -> void {
        vis.insert(bb);
        for (auto suc : bb->successors) {
            f(bb, suc);
            if (vis.count(suc)) continue;
            self(suc, f, self);
        }
    };

    dfs(bbs.front(), calc, dfs);
}

using pcalloca = const Instruction::alloca_ *;
using alloca_set = std::unordered_set<pcalloca>;
using bb2val_t = std::unordered_map<BasicBlock *, Value *>;

template <typename Func>
static void deleteLoadStore(const Function *func, const alloca_set &allocas,
                            std::unordered_map<pcalloca, Func> &find_v) {
    std::unordered_map<Value *, Value *> modified;
    const auto find_m = [&modified](Value *val, auto &&self) -> Value * {
        if (modified.count(val)) return modified[val] = self(modified[val], self);
        return val;
    };
    for (auto bb : func->bbs) {
        std::unordered_map<pcalloca, Value *> last_store;
        auto it = bb->instructions.begin();
        while (it != bb->instructions.end()) {
            auto &inst = *it;
            if (auto store = dynamic_cast<Instruction::store *>(inst);
                store && allocas.count(dynamic_cast<pcalloca>(store->getDest()))) {
                auto alloc = dynamic_cast<pcalloca>(store->getDest());
                last_store[alloc] = store->getSrc();
                it = bb->erase(store);
            } else if (auto load = dynamic_cast<Instruction::load *>(inst);
                       load && allocas.count(dynamic_cast<pcalloca>(load->getPointerOperand()))) {
                auto alloc = dynamic_cast<pcalloca>(load->getPointerOperand());
                auto &val = last_store[alloc];
                if (val == nullptr) val = find_v.at(alloc)(bb);
                auto new_val = find_m(val, find_m);
                load->moveTo(new_val);
                modified[load] = new_val;
                it = bb->erase(load);
            } else {
                ++it;
            }
        }
    }
}

static auto calcPhi(const Function *func, pcalloca alloc, bb2val_t &defs) {
    auto zero = getZero(alloc->type);
    bb2val_t liveInV;  // live in value

    // Step 1. calc defs
    std::queue<BasicBlock *> W;  // store queue
    for (auto &[bb, val] : defs) W.push(bb);

    // Step 2. mark phi
    std::unordered_set<BasicBlock *> F;  // should add phi
    while (!W.empty()) {
        auto bb = W.front();
        W.pop();
        for (auto df : bb->df) {
            if (F.count(df)) continue;
            F.insert(df);
            auto phi = new Instruction::phi(alloc->type);
            liveInV[df] = phi;
            df->push_front(phi);
            if (!defs.count(df)) W.push(df);
        }
    }

    // Step 3. create phi
    const auto find_d = [zero, &liveInV, &defs](BasicBlock *bb, auto &&self) -> Value * {
        if (defs.count(bb)) return defs[bb];
        if (liveInV.count(bb)) return defs[bb] = liveInV[bb];
        if (bb->idom == nullptr) return defs[bb] = zero;
        return defs[bb] = self(bb->idom, self);
    };
    for (auto bb : F) {
        auto phi = dynamic_cast<Instruction::phi *>(liveInV[bb]);
        assert(phi);
        for (auto pre : bb->predecessors) phi->addIncomingValue({find_d(pre, find_d), pre});
    }

    // Step 4. convert load & store
    auto find_v = [zero, liveInV = std::move(liveInV), &defs](BasicBlock *bb,
                                                              auto &&self) mutable -> Value * {
        if (liveInV.count(bb)) return liveInV[bb];
        if (bb->idom == nullptr) return liveInV[bb] = zero;
        if (defs.count(bb->idom)) return liveInV[bb] = defs[bb->idom];
        return liveInV[bb] = self(bb->idom, self);
    };
    return [find_v = std::move(find_v)](BasicBlock *bb) mutable { return find_v(bb, find_v); };
}

// 如果这一步不统一做就是 O(n2) 的，统一做事 O(nlogn)
static void calcPhi(const Function *func, const alloca_set &allocas) {
    std::unordered_map<pcalloca, bb2val_t> defs;  // def value (live out value)

    // Step 1. calc defs
    for (auto bb : func->bbs) {
        for (auto it = bb->instructions.rbegin(); it != bb->instructions.rend(); ++it) {
            if (auto store = dynamic_cast<Instruction::store *>(*it);
                store && allocas.count(dynamic_cast<pcalloca>(store->getDest()))) {
                auto alloc = dynamic_cast<pcalloca>(store->getDest());
                auto &first_store = defs[alloc][bb];
                if (first_store == nullptr) first_store = store->getSrc();
            }
        }
    }

    using Func = decltype(calcPhi(func, *allocas.begin(), defs.begin()->second));
    std::unordered_map<pcalloca, Func> find_v;
    for (auto alloca : allocas) {
        find_v.emplace(alloca, calcPhi(func, alloca, defs[alloca]));
    }
    deleteLoadStore(func, allocas, find_v);
}

void mem2reg(Function *func) {
    func->reCalcBBInfo();
    alloca_set allocas;
    for (auto inst : func->bbs.front()->instructions) {
        if (auto alloc = dynamic_cast<Instruction::alloca_ *>(inst)) {
            if (alloc->type != Type::getI32Type() && alloc->type != Type::getFloatType()) continue;
            allocas.insert(alloc);
            opt_infos.mem_to_reg()++;
        } else {
            break;
        }
    }
    calcPhi(func, allocas);
}

void clearDeadInst(const Function *func) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto bb : func->bbs) {
            auto it = bb->instructions.begin();
            while (it != bb->instructions.end()) {
                if (auto inst = *it; inst->isValue() && !inst->isUsed() && !inst->isTerminator() &&
                                     !inst->isCall()) {
                    opt_infos.clear_dead_inst()++;
                    changed = true;
                    it = bb->erase(inst);
                } else {
                    ++it;
                }
            }
        }
    }
}

void clearDeadBlock(Function *func) {
    func->calcPreSuc();
    std::unordered_set<BasicBlock *> visited{func->bbs.front()};
    std::stack<BasicBlock *> stack;
    stack.push(func->bbs.front());
    while (!stack.empty()) {
        auto bb = stack.top();
        stack.pop();
        for (auto suc : bb->successors) {
            if (visited.count(suc)) continue;
            visited.insert(suc);
            stack.push(suc);
        }
    }
    // delete block that not visited
    for (auto it = func->bbs.begin(); it != func->bbs.end();) {
        if (auto bb = *it; !visited.count(bb)) {
            for (auto &&user : bb->users())
                if (auto phi = dynamic_cast<Instruction::phi *>(user)) phi->eraseIncomingValue(bb);
            bb->moveTo(func->exitBB);  // temporary move to exitBB
            delete bb;
            opt_infos.clear_dead_block()++;
            it = func->bbs.erase(it);
        } else {
            ++it;
        }
    }
    assert(!func->exitBB->isUsed());
}

void mergeEmptyBlock(Function *func) {
    func->calcPreSuc();
    for (auto &&bb : func->bbs) {
        std::queue<BasicBlock *> check_queue;
        for (auto &&suc : bb->successors) check_queue.push(suc);
        while (!check_queue.empty()) {
            auto suc = check_queue.front();
            check_queue.pop();
            assert(bb->successors.count(suc));
            if (suc->instructions.size() > 1) continue;
            auto br = dynamic_cast<Instruction::br *>(suc->instructions.back());
            if (!br || br->hasCondition()) continue;
            auto target = br->getTarget();
            br = dynamic_cast<Instruction::br *>(bb->instructions.back());
            // should check phi here
            if (bb->successors.count(target)) {
                assert(br && br->hasCondition());
                if (!opt_settings.using_select && target->instructions.front()->isPhy()) continue;
                auto create_select = [&br](auto &&self, auto &&other) {
                    auto &&cond = br->getCondition();
                    if (br->getIfTrue() == other.second) {
                        return new Instruction::select(cond, other.first, self.first);
                    } else {
                        return new Instruction::select(cond, self.first, other.first);
                    }
                };
                for (auto &&inst : target->instructions) {
                    if (auto phi = dynamic_cast<Instruction::phi *>(inst)) {
                        auto select =
                            create_select(phi->getIncomingValue(bb), phi->getIncomingValue(suc));
                        bb->insert(br->node, select);
                        phi->substituteValue(bb, select);
                    } else {
                        break;
                    }
                }
            } else {
                for (auto &&inst : target->instructions) {
                    if (auto phi = dynamic_cast<Instruction::phi *>(inst)) {
                        auto [_v, _b] = phi->getIncomingValue(suc);
                        phi->addIncomingValue({_v, bb});
                    } else {
                        break;
                    }
                }
            }
            br->substituteOperand(suc, target);
            constantFolding(br);
            opt_infos.merge_empty_block()++;
            bb->successors.erase(suc);
            if (bb->successors.count(target)) continue;
            bb->successors.insert(target);
            target->predecessors.insert(bb);
            check_queue.push(target);
        }
    }
}
}  // namespace mir
