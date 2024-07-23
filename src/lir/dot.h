//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_LIT_DOT_H
#define COMPILER_LIT_DOT_H

#include "../dbg.h"
#ifdef _DEBUG_
#include <iostream>
#include <variant>
#include <vector>
#include "../str_helper.h"

namespace dot {
struct label {
    struct port {
        std::string port;
        std::string name;
    };
    std::variant<std::vector<label>, std::string, port> value;

    label(const std::string &str) : value(str) {}         // NOLINT
    label(const std::vector<label> &vec) : value(vec) {}  // NOLINT
    label(const port &port) : value(port) {}              // NOLINT
};

inline std::ostream &operator<<(std::ostream &os, const label &l) {
    return std::visit(
        overloaded{
            [&os](const std::string &str) -> decltype(auto) { return os << str; },
            [&os](const std::vector<label> &vec) -> decltype(auto) { return os << "{" << str::join(vec, "|") << "}"; },
            [&os](const label::port &port) -> decltype(auto) { return os << "<" << port.port << ">" << port.name; }},
        l.value);
}

struct node {
    std::string name;
    label lbl;
};

inline std::ostream &operator<<(std::ostream &os, const node &n) {
    return os << n.name << " [shape=record; label=\"" << n.lbl << "\"]";
}

enum class link_type {
    normal,
    dashed,
    red,
};

inline std::ostream &operator<<(std::ostream &os, const link_type &ty) {
    switch (ty) {
    case link_type::normal: return os << "";
    case link_type::dashed: return os << "style=dashed; color=blue";
    case link_type::red: return os << "color=red";
    }
    __builtin_unreachable();
}

struct link {
    std::string from, to;
    link_type type;
};

inline std::ostream &operator<<(std::ostream &os, const link &l) {
    return os << l.from << " -> " << l.to << " [" << l.type << "]";
}

struct graph {
    std::vector<std::variant<node, graph>> nodes;
    std::vector<link> links;
};

struct subgraph {
    static inline size_t id = 0;
    const graph &graph;
    size_t nested;
};

inline std::ostream &operator<<(std::ostream &os, const subgraph &g) {
    os << str::repeat("\t", g.nested);
    if (g.nested)
        os << "subgraph cluster_" << subgraph::id++ << " {\n";
    else
        os << "digraph {\n"
           << "\trankdir = BT;\n";
    for (const auto &n : g.graph.nodes) {
        std::visit(overloaded{
                       [&](const node &n) { os << str::repeat("\t", g.nested + 1) << n << ";"; },
                       [&](const graph &s) { os << subgraph{s, g.nested + 1}; },
                   },
                   n);
        os << "\n";
    }
    for (const auto &l : g.graph.links) os << str::repeat("\t", g.nested + 1) << l << ";\n";
    return os << str::repeat("\t", g.nested) << "}";
}

inline std::ostream &operator<<(std::ostream &os, const graph &g) { return os << subgraph{g, 0}; }
}  // namespace dot

#endif  // _DEBUG_
#endif  // COMPILER_LIT_DOT_H
