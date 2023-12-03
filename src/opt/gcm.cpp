//
// Created by toby on 2023/12/2.
//

#include "opt.h"
#include <stack>
#include <queue>

namespace mir {
    void Function::calcLoopNest() const {
        constexpr auto calc = [](BasicBlock *head, BasicBlock *tail) {
            std::unordered_set<BasicBlock *> visited{head, tail};
            std::stack<BasicBlock *> stack;
            stack.push(tail);
            while (!stack.empty()) {
                auto bb = stack.top();
                stack.pop();
                for (auto &&pre: bb->predecessors) {
                    if (visited.count(pre))
                        continue;
                    visited.insert(pre);
                    stack.push(pre);
                }
            }
            for (auto &&bb: visited)
                bb->loop_nest++;
        };

        for (auto &&bb: bbs)
            for (auto &&suc: bb->successors)
                if (bb->dominators.count(suc))
                    calc(suc, bb);
    }

    void Function::calcDomDepth() const {
        std::stack<BasicBlock *> stack;
        for (auto bb: bbs) {
            if (bb->dom_depth) continue;
            while (bb->idom) {
                stack.push(bb);
                bb = bb->idom;
            }
            while (!stack.empty()) {
                auto top = stack.top();
                stack.pop();
                top->dom_depth = top->idom->dom_depth + 1;
            }
        }
    }

    void globalCodeMotion(Function *func) {
        clearDeadBlock(func);
        func->reCalcBBInfo();
        func->calcLoopNest();
        func->calcDomDepth();
        std::vector<BasicBlock *> sorted_bbs{func->bbs.begin(), func->bbs.end()};
        std::sort(sorted_bbs.begin(), sorted_bbs.end(), [](auto &&x, auto &&y) {
            return x->dom_depth < y->dom_depth;
        });

        auto create_worklist = [&](auto &&container) {
            for (auto &&bb: sorted_bbs) {
                for (auto it = bb->beginner_end(); it != --bb->instructions.cend(); ++it)
                    if (auto &&inst = *it; !inst->isMemoryAccess() && !inst->isCall())
                        container.push(inst);
            }
        };

        // Schedule Early
        std::queue<Instruction *> work_queue;
        create_worklist(work_queue);
        while (!work_queue.empty()) {
            auto inst = work_queue.front();
            work_queue.pop();
            auto bb = inst->parent;
            while (bb->idom) {
                auto ops = inst->getOperands();
                if (std::all_of(ops.begin(), ops.end(), [&](auto &&op) {
                    auto t = dynamic_cast<Instruction *>(op);
                    return t == nullptr || bb->idom->dominators.count(t->parent);
                })) {
                    bb->idom->splice(--bb->idom->instructions.cend(), bb, inst->node);
                    bb = bb->idom;
                } else break;
            }
        }

        // LVN
        localVariableNumbering(func);

        // Schedule Late
        std::stack<Instruction *> work_stack;
        create_worklist(work_stack);
        while (!work_stack.empty()) {
            auto inst = work_stack.top();
            work_stack.pop();
            std::priority_queue<std::pair<int, BasicBlock *>> LCA;
            for (auto &&user: inst->users()) {
                auto t = dynamic_cast<Instruction *>(user);
                assert(t); // should be safe, because of user is Instruction
                if (auto phi = dynamic_cast<Instruction::phi *>(t)) {
                    for (auto i = 0; i < phi->getNumIncomingValues(); i++) {
                        auto &&[v, b] = phi->getIncomingValue(i);
                        if (v == inst) LCA.emplace(b->dom_depth, b);
                    }
                    continue;
                }
                LCA.emplace(t->parent->dom_depth, t->parent);
            }
            if (LCA.empty()) {
                inst->parent->erase(inst);
                continue;
            }
            while (LCA.size() > 1) {
                auto bb = LCA.top().second;
                LCA.pop();
                auto nxt = LCA.top().second;
                if (bb == nxt) continue;
                LCA.emplace(bb->idom->dom_depth, bb->idom);
            }
            auto lca = LCA.top().second;
            assert(lca->dominators.count(inst->parent) || (dbg(*func, inst), false));
            // find best lca
            auto best = lca;
            while (lca != inst->parent) {
                lca = lca->idom;
                if (lca->loop_nest < best->loop_nest)
                    best = lca;
            }
            if (best != inst->parent)
                best->splice(best->beginner_end(), inst->parent, inst->node);
        }
    }
}
