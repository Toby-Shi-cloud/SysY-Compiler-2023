//
// Created by toby on 2023/11/8.
//

#ifndef COMPILER_MIPS_REG_ALLOCA_H
#define COMPILER_MIPS_REG_ALLOCA_H

#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "mips/instruction.h"

namespace backend {
struct VertexInfo {
    std::unordered_set<rRegister> regs;
    std::unordered_set<rRegister> moves;
    std::unordered_set<VertexInfo *> edges;
    std::unordered_set<VertexInfo *> sub_edges;
    mips::rPhyRegister color = nullptr;
    bool freezed = false;

    [[nodiscard]] long calc_degree(const VertexInfo *other) const {
        auto set = sub_edges;
        set.insert(other->sub_edges.begin(), other->sub_edges.end());
        return static_cast<long>(set.size());
    }

    [[nodiscard]] size_t priority() const {
        if (color) return static_cast<size_t>(-1);
        size_t level = 0;
        for (auto &&reg : regs) level += 5 * reg->useUsers.size() + reg->defUsers.size();
        return level;
    }
};

struct Graph {
    using Vertex = std::unique_ptr<VertexInfo>;
    std::vector<Vertex> vertexes_pool;
    std::unordered_set<VertexInfo *> vertexes;
    std::stack<VertexInfo *> vertex_stack;
    std::unordered_map<rRegister, VertexInfo *> reg2vertex;
    std::unordered_set<rRegister> spilled_regs;

    explicit Graph(rFunction function);
    VertexInfo *get_vertex(rRegister reg);
    VertexInfo *create_vertex(rRegister reg);
    [[nodiscard]] bool can_simplify() const;
    void merge(VertexInfo *self, VertexInfo *other);
    void freeze() const;
    void simplify();
    void coalesce();
    void spill();
    void select();
};

[[nodiscard]] inline auto all_sub_blocks(rFunction function) {
    std::vector<rSubBlock> ret{};
    for (auto &block : *function)
        for (auto &sub : block->subBlocks) ret.push_back(sub.get());
    return ret;
}

void register_alloca(rFunction function);
void replace_register(rFunction function, const Graph &graph);
void compute_blocks_info(rFunction function);
void compute_use_def(rSubBlock block);
bool compute_liveIn_liveOut(rFunction function);
void compute_instructions_info(rFunction function);
bool compute_instructions_info(rSubBlock block);
}  // namespace backend

#ifdef DBG_ENABLE
namespace dbg {
template <>
inline bool pretty_print<backend::VertexInfo>(std::ostream &stream,
                                              const backend::VertexInfo &value) {
    stream << "{";
    stream << "regs: ", pretty_print(stream, value.regs), stream << ", ";
    stream << "moves: ", pretty_print(stream, value.moves), stream << ", ";
    stream << "edges: ", pretty_print(stream, value.edges), stream << ", ";
    stream << "sub_edges: ", pretty_print(stream, value.sub_edges), stream << ", ";
    stream << "color: ", pretty_print(stream, value.color), stream << ", ";
    stream << "freezed: ", pretty_print(stream, value.freezed);
    stream << "}";
    return true;
}
template <>
inline bool pretty_print<backend::VertexInfo>(std::ostream &stream,
                                              backend::VertexInfo *const &value) {
    stream << "(Vertex)";
    pretty_print(stream, value->regs);
    return true;
}
}  // namespace dbg
#endif

#endif  // COMPILER_MIPS_REG_ALLOCA_H
