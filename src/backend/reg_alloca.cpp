//
// Created by toby on 2023/11/8.
//

#include <queue>
#include <fstream>
#include <algorithm>
#include "reg_alloca.h"

#include "translator.h"

namespace backend {
    const auto alloca_regs = mips::PhyRegister::get("$t0", "$t9");
    const auto temp_regs = mips::PhyRegister::get(&mips::PhyRegister::isTemp);
    constexpr auto should_color = [](mips::rRegister r) {
        return r->isVirtual() || alloca_regs.count(dynamic_cast<mips::rPhyRegister>(r));
    };

    static mips::inst_pos_t load_at(mips::rSubBlock block, mips::inst_pos_t it, mips::rRegister dst, int offset) {
        return block->insert(it, std::make_unique<mips::LoadInst>(
                                 mips::Instruction::Ty::LW, dst, mips::PhyRegister::get("$fp"), offset));
    }

    static mips::inst_pos_t store_at(mips::rSubBlock block, mips::inst_pos_t it, mips::rRegister src, int offset) {
        return block->insert(it, std::make_unique<mips::StoreInst>(
                                 mips::Instruction::Ty::SW, src, mips::PhyRegister::get("$fp"), offset));
    }

    void register_alloca(mips::rFunction function) {
        for (;;) {
            compute_blocks_info(function);
            compute_instructions_info(function);
            Graph graph{function};
            while (graph.vertexes.size() > alloca_regs.size()) {
                while (graph.can_simplify()) {
                    graph.coalesce();
                    graph.freeze();
                    graph.simplify();
                }
                graph.spill();
            }
            graph.select();
            if (graph.spilled_regs.empty()) return replace_register(function, graph);
            // spill to memory
            for (auto reg: graph.spilled_regs) {
                assert(reg->isVirtual());
                function->allocaSize += 4;
                int offset = -static_cast<int>(function->allocaSize);
                auto vir = function->newVirRegister();
                while (!reg->useUsers.empty()) {
                    auto &&user = *reg->useUsers.begin();
                    load_at(user->parent, user->node, vir, offset);
                    reg->swapUseIn(vir, user);
                }
                while (!reg->defUsers.empty()) {
                    auto &&defer = *reg->defUsers.begin();
                    auto it = defer->node;
                    store_at(defer->parent, ++it, vir, offset);
                    reg->swapDefIn(vir, defer);
                }
            }
        }
    }

    void replace_register(mips::rFunction function, const Graph &graph) {
        for (auto &&vertex: graph.vertexes_pool) {
            bool used = false;
            for (auto &&reg: vertex->regs)
                if (reg->isVirtual()) {
                    reg->swapTo(vertex->color);
                    used = true;
                }
            if (used && vertex->color->isSaved())
                function->shouldSave.insert(vertex->color);
        }
    }

    void compute_blocks_info(mips::rFunction function) {
        // 1. compute pre and suc
        function->calcBlockPreSuc();
        // 2. compute use and def
        for (auto &block: all_sub_blocks(function))
            compute_use_def(block);
        // 3. compute liveIn and liveOut
        for (auto &block: all_sub_blocks(function)) {
            block->liveIn.insert(block->use.begin(), block->use.end());
            block->liveIn.erase(mips::PhyRegister::get(0));
        }
        while (compute_liveIn_liveOut(function));
    }

    void compute_use_def(mips::rSubBlock block) {
        for (auto &inst: *block) {
            for (auto reg: inst->regUse)
                if (block->def.count(reg) == 0)
                    block->use.insert(reg);
            for (auto reg: inst->regDef)
                if (block->use.count(reg) == 0)
                    block->def.insert(reg);
        }
    }

    bool compute_liveIn_liveOut(mips::rFunction function) {
        bool changed = false;
        auto vec = all_sub_blocks(function);
        for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
            auto block = *it;
            auto s1 = block->liveIn.size();
            auto s2 = block->liveOut.size();
            for (auto suc: block->successors)
                block->liveOut.insert(suc->liveIn.begin(), suc->liveIn.end());
            block->liveIn.insert(block->liveOut.begin(), block->liveOut.end());
            for (auto reg: block->def)
                block->liveIn.erase(reg);
            if (s1 != block->liveIn.size() || s2 != block->liveOut.size())
                changed = true;
        }
        return changed;
    }

    void compute_instructions_info(mips::rFunction function) {
        for (auto &&block: all_sub_blocks(function))
            for (auto &&inst: *block)
                inst->liveIn.clear(), inst->liveOut.clear();
        for (auto &&block: all_sub_blocks(function))
            while (compute_instructions_info(block));
    }

    bool compute_instructions_info(mips::rSubBlock block) {
        bool changed = false;
        for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
            auto &&inst = *it;
            auto suc = inst->next();
            auto s1 = inst->liveIn.size();
            auto s2 = inst->liveOut.size();
            if (it != block->instructions.rbegin())
                inst->liveOut.insert(suc->liveIn.begin(), suc->liveIn.end());
            else
                inst->liveOut.insert(block->liveOut.begin(), block->liveOut.end());
            inst->liveIn.insert(inst->liveOut.begin(), inst->liveOut.end());
            for (auto reg: inst->regDef)
                inst->liveIn.erase(reg);
            inst->liveIn.insert(inst->regUse.begin(), inst->regUse.end());
            if (s1 != inst->liveIn.size() || s2 != inst->liveOut.size())
                changed = true;
        }
        return changed;
    }

    void Graph::merge(VertexInfo *self, VertexInfo *other) {
        assert(!self->edges.count(other) && !other->edges.count(self));
        assert(!(self->color && other->color));
        assert(!self->freezed && !other->freezed);
        assert(self->calc_degree(other) < alloca_regs.size());
        assert(vertexes.count(self) && vertexes.count(other));
        self->regs.insert(other->regs.begin(), other->regs.end());
        self->moves.insert(other->moves.begin(), other->moves.end());
        self->edges.insert(other->edges.begin(), other->edges.end());
        self->sub_edges.insert(other->sub_edges.begin(), other->sub_edges.end());
        self->color = self->color ? self->color : other->color;
        for (auto reg: self->regs) self->moves.erase(reg);
        for (auto reg: other->regs) reg2vertex[reg] = self;
        for (auto vertex: other->edges) vertex->edges.erase(other), vertex->edges.insert(self);
        for (auto vertex: other->sub_edges) vertex->sub_edges.erase(other), vertex->sub_edges.insert(self);
        vertexes.erase(other);
        *other = {};
    }

    Graph::Graph(mips::rFunction function) {
        const auto conflict = [this](auto &&set1, auto &&set2) {
            for (auto &&u: set1)
                if (should_color(u) && get_vertex(u))
                    for (auto &&v: set2)
                        if (should_color(v) && u != v) {
                            get_vertex(u)->edges.insert(get_vertex(v));
                            get_vertex(u)->sub_edges.insert(get_vertex(v));
                        }
        };

        for (auto &&block: all_sub_blocks(function)) {
            for (auto &&inst: *block) {
                conflict(inst->liveIn, inst->liveIn);
                conflict(inst->liveOut, inst->liveOut);
                if (inst->isFuncCall())
                    conflict(inst->liveOut, temp_regs), conflict(temp_regs, inst->liveOut);
            }
        }

        conflict(alloca_regs, alloca_regs);
    }

    VertexInfo *Graph::get_vertex(mips::rRegister reg) {
        if (auto v = reg2vertex[reg]) return v;
        return create_vertex(reg);
    }

    VertexInfo *Graph::create_vertex(mips::rRegister reg) {
        assert(should_color(reg));
        auto vertex = new VertexInfo();
        vertex->regs.insert(reg);
        reg2vertex[reg] = vertex;
        vertex->color = dynamic_cast<mips::rPhyRegister>(reg);
        for (auto &&inst: reg->defUsers)
            if (auto move = dynamic_cast<mips::rMoveInst>(inst);
                move && should_color(move->src()))
                vertex->moves.insert(move->src());
        for (auto &&inst: reg->useUsers)
            if (auto move = dynamic_cast<mips::rMoveInst>(inst);
                move && should_color(move->dst()))
                vertex->moves.insert(move->dst());
        vertexes.insert(vertex);
        vertexes_pool.emplace_back(vertex);
        return vertex;
    }

    bool Graph::can_simplify() const {
        return std::any_of(vertexes.begin(), vertexes.end(), [](auto &&vertex) {
            return !vertex->color && vertex->sub_edges.size() < alloca_regs.size();
        });
    }

    void Graph::freeze() const {
        int freeze_cnt = 0;
        for (auto &&vertex: vertexes)
            if (!vertex->freezed && !vertex->color && vertex->moves.empty()) {
                vertex->freezed = true;
                freeze_cnt += 1;
            }
        if (freeze_cnt) return;
        for (auto &&vertex: vertexes)
            if (!vertex->freezed && !vertex->color
                && vertex->sub_edges.size() >= alloca_regs.size() * 2 / 3
                && vertex->sub_edges.size() < alloca_regs.size()) {
                vertex->freezed = true;
                freeze_cnt += 1;
            }
        if (freeze_cnt) return;
        for (auto &&vertex: vertexes)
            if (!vertex->freezed && !vertex->color
                && vertex->sub_edges.size() >= alloca_regs.size() / 2
                && vertex->sub_edges.size() < alloca_regs.size()) {
                vertex->freezed = true;
                freeze_cnt += 1;
            }
        if (freeze_cnt) return;
        for (auto &&vertex: vertexes)
            if (!vertex->freezed && !vertex->color
                && vertex->sub_edges.size() >= alloca_regs.size()) {
                vertex->freezed = true;
                freeze_cnt += 1;
            }
    }

    void Graph::simplify() {
        constexpr auto pred =
                [](auto &&vertex) { return vertex->freezed && vertex->sub_edges.size() < alloca_regs.size(); };

        std::queue<VertexInfo *> queue;
        for (auto it = vertexes.begin(); it != vertexes.end();) {
            if (auto vertex = *it; pred(vertex)) {
                queue.push(vertex);
                it = vertexes.erase(it);
            } else ++it;
        }

        while (!queue.empty()) {
            auto u = queue.front();
            queue.pop();
            vertex_stack.push(u);
            for (auto v: u->sub_edges) {
                if (!vertexes.count(v)) continue;
                v->sub_edges.erase(u);
                if (pred(v)) {
                    queue.push(v);
                    vertexes.erase(v);
                }
            }
        }
    }

    void Graph::coalesce() {
        for (;;) {
            long max_degree = -1;
            std::pair<VertexInfo *, VertexInfo *> pair;
            for (auto &&vertex: vertexes)
                for (auto it = vertex->moves.begin(); it != vertex->moves.end();) {
                    if (!vertexes.count(reg2vertex.at(*it))
                        || reg2vertex.at(*it)->freezed
                        || vertex->regs.count(*it)
                        || vertex->edges.count(reg2vertex.at(*it))) {
                        it = vertex->moves.erase(it);
                        continue;
                    }
                    if (auto degree = vertex->calc_degree(reg2vertex.at(*it));
                        degree < alloca_regs.size() && degree > max_degree) {
                        max_degree = degree;
                        pair = {vertex, reg2vertex.at(*it)};
                    }
                    ++it;
                }
            if (max_degree != -1) merge(pair.first, pair.second);
            else break;
        }
    }

    void Graph::spill() {
        auto vertex = *std::min_element(vertexes.begin(), vertexes.end(),
                                        [](auto &&u, auto &&v) { return u->priority() < v->priority(); });
        vertexes.erase(vertex);
        vertex_stack.push(vertex);
        for (auto v: vertex->sub_edges) {
            assert(vertexes.count(v) || (dbg(*vertex, *v), false));
            v->sub_edges.erase(vertex);
        }
    }

    void Graph::select() {
        while (!vertex_stack.empty()) {
            auto u = vertex_stack.top();
            vertex_stack.pop();
            auto avail = alloca_regs;
            for (auto v: u->edges)
                avail.erase(v->color);
            if (avail.empty()) spilled_regs.insert(u->regs.begin(), u->regs.end());
            else u->color = *avail.begin();
        }
    }
}
