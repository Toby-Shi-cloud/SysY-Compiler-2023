//
// Created by toby on 2024/6/21.
//

#ifndef COMPILER_LIR_DAG_H
#define COMPILER_LIR_DAG_H

#include <list>
#include <vector>
#include <variant>
#include <sstream>
#include "dot.h"
#include "../mir.h"
#include "../slice.h"

#if ARCHITECTURE_XLEN == 32
#define DAG_ADDRESS_TYPE mir::Type::getI32Type()
#define LIR lir32
#elif ARCHITECTURE_XLEN == 64
#define DAG_ADDRESS_TYPE mir::Type::getI64Type()
#define LIR lir64
#else
#error "Unsupported XLEN"
#endif

namespace LIR {
    struct DAGNode;
    struct DAGLink;
    using DAG = std::list<std::unique_ptr<DAGNode>>;

    struct DAGValue {
        using Value = mir::pType;
        struct Ch {};
        struct Glue {};
        using Type = std::variant<Value, Ch, Glue>;

        Type type;
        DAGNode *node;
        std::list<DAGLink *> users;

        DAGValue(DAGNode *node, Type type) : type(type), node(node) {}

        inline ~DAGValue();

        inline DAGValue *last_user_ch();

        inline void swap_to(DAGValue *that);
    };

    inline std::string to_string(const DAGValue::Type &ty) {
        return std::visit(overloaded{
            [](const DAGValue::Value &val) -> std::string {
                return val->to_string();
            },
            [](const DAGValue::Ch &) -> std::string {
                return "ch";
            },
            [](const DAGValue::Glue &) -> std::string {
                return "glue";
            }
        }, ty);
    }

    inline bool operator==(const DAGValue::Type &lhs, const DAGValue::Type &rhs) {
        if (lhs.index() != rhs.index()) return false;
        if (lhs.index() == 0) return std::get<0>(lhs) == std::get<0>(rhs);
        return true;
    }

    inline bool operator!=(const DAGValue::Type &lhs, const DAGValue::Type &rhs) {
        return !(lhs == rhs);
    }

    class DAGLink {
        using Position = std::list<DAGLink *>::iterator;
        DAGValue *m_value = nullptr;
        Position m_user_pos;

    public:
        DAGValue::Type type;
        DAGNode *node;

        DAGLink(DAGNode *node, DAGValue::Type type) : node(node), type(type) {}

        ~DAGLink() { reset(); }

        [[nodiscard]] DAGValue *value() const { return m_value; }

        [[nodiscard]] Position user_pos() const { return m_user_pos; }

        void reset() {
            if (m_value) m_value->users.erase(m_user_pos);
            m_value = nullptr;
        }

        void set_link(DAGValue *target) {
            reset();
            if (target == nullptr) return;
            assert(target->type == type);
            m_value = target;
            m_user_pos = m_value->users.insert(m_value->users.end(), this);
        }
    };

    /// Directed Acyclic Graph Node
    /// This is the basic node and contains nothing but useful method.
    struct DAGNode {
        virtual ~DAGNode() = default;

        [[nodiscard]] virtual const_slice<DAGLink> links() const = 0;

        [[nodiscard]] virtual const_slice<DAGValue> values() const = 0;

        [[nodiscard]] virtual std::string name() const = 0;

        [[nodiscard]] virtual DAGValue *ch_value() { return nullptr; };

        [[nodiscard]] virtual bool in_used() const {
            auto v = values();
            return std::any_of(v.begin(), v.end(), [](auto &&v) {
                return !v.users.empty();
            });
        }

#ifdef _DEBUG_
        std::string m_node_id;

        void set_original(std::string node_id) {
            m_node_id = std::move(node_id);
        }

        [[nodiscard]] auto to_dot_node_name() const {
            std::stringstream ss;
            ss << "Node0x" << std::ios::hex << reinterpret_cast<uintptr_t>(this);
            return ss.str();
        }

        [[nodiscard]] auto to_dot_node() const {
            auto my_links = links();
            std::vector<dot::label> labels;
            std::vector<dot::link> links;
            if (!my_links.empty()) {
                std::vector<dot::label> link_labels;
                link_labels.reserve(my_links.size());
                for (auto &&l: my_links) {
                    auto port_name = "i" + std::to_string(link_labels.size());
                    link_labels.emplace_back(dot::label::port{
                        port_name,
                        to_string(l.type)
                    });
                    auto v = l.value();
                    if (v == nullptr) continue;
                    links.emplace_back(dot::link{
                        to_dot_node_name() + ":" + port_name,
                        v->node->to_dot_node_name() + ":o" + std::to_string(v - v->node->values().begin()),
                        std::visit(overloaded{
                            [](const DAGValue::Value &) { return dot::link_type::normal; },
                            [](const DAGValue::Ch &) { return dot::link_type::dashed; },
                            [](const DAGValue::Glue &) { return dot::link_type::red; }
                        }, l.type),
                    });
                }
                labels.emplace_back(link_labels);
            }
            labels.emplace_back(name());
            if (!m_node_id.empty()) labels.emplace_back(m_node_id);
            if (!values().empty()) {
                std::vector<dot::label> value_labels;
                value_labels.reserve(values().size());
                for (auto &&v: values()) {
                    value_labels.emplace_back(dot::label::port{
                        "o" + std::to_string(value_labels.size()),
                        to_string(v.type)
                    });
                }
                labels.emplace_back(value_labels);
            }
            return std::make_pair(dot::node{to_dot_node_name(), {labels}}, links);
        }
#endif
    };

    inline DAGValue::~DAGValue() {
        for (auto &&u: std::list{users}) u->reset();
    }

    inline DAGValue *DAGValue::last_user_ch() {
        if (users.empty()) return nullptr;
        return users.back()->node->ch_value();
    }

    inline void DAGValue::swap_to(DAGValue *that) {
        for (auto it = users.begin(); it != users.end();) {
            auto link = *(it++);
            link->set_link(that);
        }
        assert(users.empty());
    }
}

namespace LIR::node {
/// MARKER: DAGNode Computation

#define DAG_NODE_GEN(ty) \
    struct ty : DAGNode { \
        DAGLink lhs, rhs; \
        DAGValue ret; \
        explicit ty(DAGValue::Type type) : ty(type, type, type) {} \
        ty(DAGValue::Type lhs, DAGValue::Type rhs, DAGValue::Type ret) : lhs(this, lhs), rhs(this, rhs), ret(this, ret) {} \
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&lhs, 2}; } \
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ret, 1}; } \
        [[nodiscard]] std::string name() const override { return #ty; } \
    }

    DAG_NODE_GEN(add);
    DAG_NODE_GEN(sub);
    DAG_NODE_GEN(mul);
    DAG_NODE_GEN(udiv);
    DAG_NODE_GEN(sdiv);
    DAG_NODE_GEN(urem);
    DAG_NODE_GEN(srem);

    DAG_NODE_GEN(fadd);
    DAG_NODE_GEN(fsub);
    DAG_NODE_GEN(fmul);
    DAG_NODE_GEN(fdiv);
    DAG_NODE_GEN(frem);

    DAG_NODE_GEN(shl);
    DAG_NODE_GEN(lshr);
    DAG_NODE_GEN(ashr);
    DAG_NODE_GEN(and_);
    DAG_NODE_GEN(or_);
    DAG_NODE_GEN(xor_);

#undef DAG_NODE_GEN

#define DAG_NODE_GEN(ty) \
    struct ty : DAGNode { \
        DAGLink val; \
        DAGValue ret; \
        explicit ty(DAGValue::Type type) : ty(type, type) {} \
        ty(DAGValue::Type val, DAGValue::Type ret) : val(this, val), ret(this, ret) {} \
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&val, 1}; } \
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ret, 1}; } \
        [[nodiscard]] std::string name() const override { return #ty; } \
    }

    DAG_NODE_GEN(fneg);
    DAG_NODE_GEN(zext);
    DAG_NODE_GEN(sext);
    DAG_NODE_GEN(trunc);
    DAG_NODE_GEN(fptoui);
    DAG_NODE_GEN(fptosi);
    DAG_NODE_GEN(uitofp);
    DAG_NODE_GEN(sitofp);

#undef DAG_NODE_GEN

/// MARKER: DAGNode Transformation

    struct LoadNode : DAGNode {
        DAGLink dependency{this, DAGValue::Ch()}, address{this, DAG_ADDRESS_TYPE};
        DAGValue value, ch{this, DAGValue::Ch()};
        explicit LoadNode(DAGValue::Type type) : value(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&dependency, 2}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&value, 2}; }
        [[nodiscard]] std::string name() const override { return "load"; }
        [[nodiscard]] DAGValue *ch_value() override { return &ch; };
    };

    struct StoreNode : DAGNode {
        DAGLink dependency{this, DAGValue::Ch()}, value, address{this, DAG_ADDRESS_TYPE};
        DAGValue ch{this, DAGValue::Ch()};
        explicit StoreNode(DAGValue::Type type) : value(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&dependency, 3}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ch, 1}; }
        [[nodiscard]] std::string name() const override { return "store"; }
        [[nodiscard]] DAGValue *ch_value() override { return &ch; };
    };

    struct CopyToReg : DAGNode {
        DAGLink dependency{this, DAGValue::Ch()}, reg, value;
        DAGValue ch{this, DAGValue::Ch()}, glue{this, DAGValue::Glue()};
        explicit CopyToReg(DAGValue::Type type) : reg(this, type), value(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&dependency, 3}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ch, 2}; }
        [[nodiscard]] std::string name() const override { return "CopyToReg"; }
        [[nodiscard]] DAGValue *ch_value() override { return &ch; };
    };

    struct CopyFromReg : DAGNode {
        DAGLink dependency{this, DAGValue::Ch()}, reg, glue{this, DAGValue::Glue()};
        DAGValue value, ch{this, DAGValue::Ch()};
        explicit CopyFromReg(DAGValue::Type type) : reg(this, type), value(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&dependency, 3}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&value, 2}; }
        [[nodiscard]] std::string name() const override { return "CopyFromReg"; }
        [[nodiscard]] DAGValue *ch_value() override { return &ch; };
    };

/// MARKER: DAGNode Value

    struct DAGValueNode : DAGNode {
        DAGValue value;
        explicit DAGValueNode(DAGValue::Type type): value(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {nullptr, 0}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&value, 1}; }
    };

    struct VRegister : DAGValueNode {
        size_t v_id;
        VRegister(size_t v_id, DAGValue::Type type) : DAGValueNode(type), v_id(v_id) {}
        [[nodiscard]] std::string name() const override { return "VRegister %" + std::to_string(v_id); }
    };

    struct Constant : DAGValueNode {
        struct value_t : std::variant<float, long long> {
            using variant::variant;

            explicit value_t(mir::calculate_t v) noexcept {
                std::visit(overloaded{
                    [this](float v) { emplace<0>(v); },
                    [this](int v) { emplace<1>(v); },
                }, v);
            }

            template<typename T>
            explicit operator T() const {
                return std::visit([](auto v) { return (T) v; }, *this);
            }

            std::string to_string() const {
                return std::visit([](auto v) { return std::to_string(v); }, *this);
            }
        } constant;
        explicit Constant(mir::Literal *literal) : DAGValueNode(literal->type), constant(literal->getValue()) {}
        Constant(DAGValue::Type type, value_t constant) : DAGValueNode(type), constant(constant) {}
        [[nodiscard]] std::string name() const override { return "constant\\<" + constant.to_string() + "\\>"; }
    };

    struct GAddress : DAGValueNode {
        mir::GlobalVar *g_var;
        explicit GAddress(mir::GlobalVar *g_var) : DAGValueNode(DAG_ADDRESS_TYPE), g_var(g_var) {}
        [[nodiscard]] std::string name() const override { return "global\\<" + g_var->name + "\\>"; }
    };

    struct LAddress : DAGValueNode {
        ssize_t offset; // offset to fp (positive for params & negative for local val)
        explicit LAddress(ssize_t offset) : DAGValueNode(DAG_ADDRESS_TYPE), offset(offset) {}
        [[nodiscard]] std::string name() const override { return std::to_string(offset) + "(fp)"; }
    };

/// MARKER: DAGNode Control Flow

    struct PhiNode : DAGNode {
        std::vector<DAGLink> ops;
        DAGValue ret;
        explicit PhiNode(DAGValue::Type type) : ret(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {ops.data(), ops.size()}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ret, 1}; }
        [[nodiscard]] std::string name() const override { return "phi"; }
    };

    struct SelectNode : DAGNode {
        DAGLink cond{this, mir::Type::getI1Type()}, lhs, rhs;
        DAGValue ret;
        explicit SelectNode(DAGValue::Type type) : lhs(this, type), rhs(this, type), ret(this, type) {}
        [[nodiscard]] const_slice<DAGLink> links() const override { return {&cond, 3}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ret, 1}; }
        [[nodiscard]] std::string name() const override { return "select"; }
    };

    struct EntryToken : DAGNode {
        DAGValue ch{this, DAGValue::Ch()};
        [[nodiscard]] const_slice<DAGLink> links() const override { return {nullptr, 0}; }
        [[nodiscard]] const_slice<DAGValue> values() const override { return {&ch, 1}; }
        [[nodiscard]] std::string name() const override { return "EntryToken"; }
        [[nodiscard]] DAGValue *ch_value() override { return &ch; };
    };

/// Note: architecture specific nodes are NOT defined here.
}

#ifdef _DEBUG_
namespace LIR {
    inline dot::graph to_graph(const std::vector<DAG> &dag) {
        dot::graph graph;
        for (auto &&d: dag) {
            dot::graph sub;
            for (auto &&n: d) {
                auto [node, links] = n->to_dot_node();
                sub.nodes.emplace_back(node);
                sub.links.insert(sub.links.end(), links.begin(), links.end());
            }
            graph.nodes.emplace_back(sub);
        }
        return graph;
    }
}
#endif

#endif //COMPILER_LIR_DAG_H
