//
// Created by toby on 2024/6/23.
//

#ifndef COMPILER_LIR_CONSTANT_H
#define COMPILER_LIR_CONSTANT_H

#include <cmath>
#include <optional>
#include <queue>
#include "lir/dag.h"

namespace LIR {
inline std::optional<node::Constant::value_t> get_constant(DAGLink *link) {
    if (auto node = dynamic_cast<node::Constant *>(link->value()->node)) {
        return node->constant;
    }
    return std::nullopt;
}

template <typename Node>
node::Constant::value_t constant_folding_impl(node::Constant::value_t lhs, node::Constant::value_t rhs);

template <typename Node>
node::Constant::value_t constant_folding_impl(node::Constant::value_t val);

#define LL long long
#define ULL unsigned long long
#define FOLD(ty, op, tout, tin)                                                                 \
    template <> inline node::Constant::value_t                                                  \
    constant_folding_impl<node::ty>(node::Constant::value_t lhs, node::Constant::value_t rhs) { \
        return (tout)((tin)(lhs)op(tin)(rhs));                                                  \
    }
FOLD(add, +, LL, LL)
FOLD(sub, -, LL, LL)
FOLD(mul, *, LL, LL)
FOLD(sdiv, /, LL, LL)
FOLD(udiv, /, LL, ULL)
FOLD(srem, %, LL, LL)
FOLD(urem, %, LL, ULL)
FOLD(fadd, +, float, float)
FOLD(fsub, -, float, float)
FOLD(fmul, *, float, float)
FOLD(fdiv, /, float, float)
FOLD(shl, <<, LL, LL)
FOLD(ashr, >>, LL, LL)
FOLD(lshr, >>, LL, ULL)
FOLD(or_, |, LL, LL)
FOLD(and_, &, LL, LL)
FOLD(xor_, ^, LL, LL)
#undef FOLD

template <>
inline node::Constant::value_t constant_folding_impl<node::frem>(node::Constant::value_t lhs,
                                                                 node::Constant::value_t rhs) {
    return std::fmod((float)lhs, (float)rhs);
}

#define FOLD(ty) \
    template <>  \
    inline node::Constant::value_t constant_folding_impl<node::ty>(node::Constant::value_t val)
FOLD(fneg) { return -(float)val; }
FOLD(zext) { return val; }
FOLD(sext) { return val; }
FOLD(trunc) { return val; }
FOLD(fptoui) { return (LL)(ULL)(float)val; }
FOLD(fptosi) { return (LL)(float)val; }
FOLD(uitofp) { return (float)(ULL)val; }
FOLD(sitofp) { return (float)(LL)val; }
#undef FOLD
#undef ULL
#undef LL

// binary node
template <typename Node>
auto constant_folding(Node *node) -> decltype(node->lhs, node->rhs, std::unique_ptr<DAGNode>(nullptr)) {
    auto lhs = get_constant(&node->lhs);
    auto rhs = get_constant(&node->rhs);
    if (!lhs || !rhs) return nullptr;
    node->lhs.reset(), node->rhs.reset();
    auto c = constant_folding_impl<Node>(*lhs, *rhs);
    auto n = std::make_unique<node::Constant>(node->ret.type, c);
    node->ret.swap_to(&n->value);
    return n;
}

// specific for address computation
template <>
inline std::unique_ptr<DAGNode> constant_folding<node::add>(node::add *node) {
    auto lhs = get_constant(&node->lhs);
    auto rhs = get_constant(&node->rhs);
    std::unique_ptr<node::DAGValueNode> n = nullptr;
    if (lhs && rhs) {
        auto c = constant_folding_impl<node::add>(*lhs, *rhs);
        n = std::make_unique<node::Constant>(node->ret.type, c);
    } else if (auto rhs_addr = dynamic_cast<node::LAddress *>(node->rhs.value()->node); lhs && rhs_addr) {
        n = std::make_unique<node::LAddress>(rhs_addr->offset + (ssize_t)*lhs);
    } else if (auto lhs_addr = dynamic_cast<node::LAddress *>(node->lhs.value()->node); rhs && lhs_addr) {
        n = std::make_unique<node::LAddress>(lhs_addr->offset + (ssize_t)*rhs);
    }
    if (!n) return nullptr;
    node->lhs.reset(), node->rhs.reset();
    node->ret.swap_to(&n->value);
    return n;
}

// specific for address computation
template <>
inline std::unique_ptr<DAGNode> constant_folding<node::sub>(node::sub *node) {
    auto lhs = get_constant(&node->lhs);
    auto rhs = get_constant(&node->rhs);
    std::unique_ptr<node::DAGValueNode> n = nullptr;
    if (lhs && rhs) {
        auto c = constant_folding_impl<node::sub>(*lhs, *rhs);
        n = std::make_unique<node::Constant>(node->ret.type, c);
    } else if (auto lhs_addr = dynamic_cast<node::LAddress *>(node->lhs.value()->node); rhs && lhs_addr) {
        n = std::make_unique<node::LAddress>(lhs_addr->offset - (ssize_t)*rhs);
    }
    if (!n) return nullptr;
    node->lhs.reset(), node->rhs.reset();
    node->ret.swap_to(&n->value);
    return n;
}

// unary node
template <typename Node>
auto constant_folding(Node *node) -> decltype(node->val, std::unique_ptr<DAGNode>(nullptr)) {
    auto val = get_constant(&node->val);
    if (!val) return nullptr;
    node->val.reset();
    auto c = constant_folding_impl<Node>(*val);
    auto n = std::make_unique<node::Constant>(node->ret.type, c);
    node->ret.swap_to(&n->value);
    return n;
}

[[nodiscard]] inline auto constant_folding(DAGNode *node) {
#define TRY(ty) \
    if (auto val = dynamic_cast<node::ty *>(node)) return constant_folding(val)
    FOR_EACH(TRY, add, sub, mul, sdiv, udiv, srem, urem);
    FOR_EACH(TRY, fadd, fsub, fmul, fdiv, frem, fneg);
    FOR_EACH(TRY, shl, lshr, ashr, and_, or_, xor_);
    FOR_EACH(TRY, zext, sext, trunc, fptoui, fptosi, uitofp, sitofp);
#undef TRY
    return std::unique_ptr<DAGNode>(nullptr);
}

inline void constant_folding(DAG &dag) {
    std::queue<DAG::iterator> queue;
    for (auto it = dag.begin(); it != dag.end(); ++it) {
        if (dynamic_cast<node::Constant *>(it->get())) queue.push(it);
    }
    while (!queue.empty()) {
        auto front = queue.front();
        queue.pop();
        auto c = dynamic_cast<node::Constant *>(front->get());
        if (!c) continue;
        for (auto it = c->value.users.begin(); it != c->value.users.end();) {
            auto ptr = *(it++);
            auto node = constant_folding(ptr->node);
            if (node == nullptr) continue;
            dag.push_front(std::move(node));
            queue.push(dag.begin());
        }
        if (c->value.users.empty()) dag.erase(front);
    }
    for (auto it = dag.begin(); it != dag.end();) {
        if (it->get()->in_used())
            ++it;
        else
            it = dag.erase(it);
    }
}
}  // namespace LIR

#endif  // COMPILER_LIR_CONSTANT_H
