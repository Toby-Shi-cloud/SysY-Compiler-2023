//
// Created by toby on 2023/11/26.
//

#include <functional>
#include <set>
#include "opt.h"

using value_vector_t = std::vector<mir::Value *>;
using inst_vector_t = std::vector<mir::Instruction *>;
using operator_t = std::pair<mir::Instruction::InstrTy, value_vector_t>;

template <>
struct [[maybe_unused]] std::hash<value_vector_t> {
    static constexpr auto _hash = std::hash<mir::Value *>();

    size_t operator()(const value_vector_t &v) const noexcept {
        size_t ret = 0;
        for (const auto &e : v) ret = ret * 10007 + _hash(e);
        return ret;
    }
};

template <>
struct [[maybe_unused]] std::hash<operator_t> {
    static constexpr auto _hash_0 = std::hash<mir::Instruction::InstrTy>();
    static constexpr auto _hash_1 = std::hash<value_vector_t>();

    size_t operator()(const operator_t &v) const noexcept {
        return _hash_0(v.first) ^ _hash_1(v.second);
    }
};

namespace mir {
using variable_map_t = std::unordered_map<operator_t, Value *>;
inline std::unordered_map<BasicBlock *, variable_map_t> variables;

inline Value *find(BasicBlock *bb, const operator_t &key) {
    if (bb == nullptr) return nullptr;
    if (variables[bb].count(key)) return variables[bb][key];
    return variables[bb][key] = find(bb->idom, key);
}

inline bool irrelevant(const root_value_t &x, const root_value_t &y) {
    if (dynamic_cast<const Argument *>(x.first) &&
        !dynamic_cast<const Instruction::alloca_ *>(y.first))
        return false;  // argument is always relevant to global variable and other arguments
    if (dynamic_cast<const Argument *>(y.first) &&
        !dynamic_cast<const Instruction::alloca_ *>(x.first))
        return false;
    if (x.first != y.first) return true;
    if (!x.second || !y.second) return false;
    return *x.second != *y.second;
}

inline std::optional<Value *> isUseless(const BasicBlock *bb, const Instruction::load *current) {
    auto root = getRootValue(current);
    auto it = current->node;
    while (it != bb->instructions.begin()) {
        auto &&inst = *--it;
        if (isPureInst(inst)) continue;
        if (auto call = dynamic_cast<Instruction::call *>(inst)) {
            if (call->getFunction()->noPostEffect) continue;
            return std::nullopt;
        }
        if (auto store = dynamic_cast<Instruction::store *>(inst)) {
            if (store->getDest() == current->getPointerOperand()) return store->getSrc();
            if (irrelevant(getRootValue(store), root)) continue;
            return std::nullopt;
        }
        if (auto memset = dynamic_cast<Instruction::memset *>(inst)) {
            if (irrelevant(getRootValue(memset), root)) continue;
            return std::nullopt;
        }
        auto other = dynamic_cast<Instruction::load *>(inst);
        assert(other);
        if (other->getPointerOperand() == current->getPointerOperand()) return other;
    }
    return std::nullopt;
}

inline std::optional<Value *> isUseless(const BasicBlock *bb, const Instruction::store *current) {
    auto root = getRootValue(current);
    auto it = current->node;
    while (it != bb->instructions.begin()) {
        auto &&inst = *--it;
        if (auto load = dynamic_cast<Instruction::load *>(inst))
            if (load->getPointerOperand() == current->getDest() && load == current->getSrc())
                return nullptr;
        if (auto other = dynamic_cast<Instruction::store *>(inst))
            if (!irrelevant(getRootValue(other), root)) break;
        if (auto memset = dynamic_cast<Instruction::memset *>(inst))
            if (!irrelevant(getRootValue(memset), root)) break;
    }
    it = std::next(current->node);
    for (; it != bb->instructions.end(); ++it) {
        auto &&inst = *it;
        if (isPureInst(inst)) continue;
        if (inst->isCall()) return std::nullopt;
        if (auto load = dynamic_cast<Instruction::load *>(inst)) {
            if (irrelevant(getRootValue(load), root)) continue;
            return std::nullopt;
        }
        auto other = dynamic_cast<Instruction::store *>(inst);
        assert(other);
        if (other->getDest() == current->getDest()) return nullptr;
    }
    return std::nullopt;
}

inline void globalVariableNumbering(BasicBlock *bb) {
    std::unordered_map<Instruction *, inst_vector_t> edges;
    std::unordered_map<Instruction *, int> degrees;
    std::unordered_map<Instruction *, int> origin;
    int cnt = 0;
    auto _isUseless = [&bb](auto &&inst) -> std::optional<Value *> {
        if (auto load = dynamic_cast<const Instruction::load *>(inst)) return isUseless(bb, load);
        if (auto store = dynamic_cast<const Instruction::store *>(inst))
            return isUseless(bb, store);
        return std::nullopt;
    };

    std::vector<Instruction *> memory_inst;
    auto _last_relevant = [&](auto &&inst) -> Value * {
        auto root = getRootValue(inst);
        auto self = dynamic_cast<Instruction::call *>(inst);
        for (auto it = memory_inst.rbegin(); it != memory_inst.rend(); ++it) {
            auto &&other = *it;
            if (auto load = dynamic_cast<Instruction::load *>(other))
                if (self && !self->getFunction()->noPostEffect ||
                    !irrelevant(getRootValue(load), root))
                    return load;
            if (auto store = dynamic_cast<Instruction::store *>(other))
                if (self || !irrelevant(getRootValue(other), root)) return store;
            if (auto call = dynamic_cast<Instruction::call *>(other))
                if (!call->getFunction()->noPostEffect || inst->instrTy == Instruction::STORE)
                    return call;
        }
        return bb;
    };

    for (auto it = bb->beginner_end(); it != std::prev(bb->instructions.cend());) {
        auto inst = *it;
        auto values = inst->getOperands();
        if (auto _value = _isUseless(inst)) {
            opt_infos.global_variable_numbering()++;
            if (auto value = *_value)
                it = substitute(inst, value);
            else
                it = bb->erase(inst);
            continue;
        }
        if (!isPureInst(inst)) {
            values.push_back(_last_relevant(inst));
            memory_inst.push_back(inst);
        }
        if (auto icmp = dynamic_cast<Instruction::icmp *>(inst))
            values.push_back(getIntegerLiteral(icmp->cond));
        operator_t key = {inst->instrTy, std::move(values)};
        if (auto value = find(bb, key)) {
            opt_infos.global_variable_numbering()++;
            it = substitute(inst, value);
            continue;
        }
        variables[bb][key] = inst;
        degrees[inst] = 0;
        origin[inst] = cnt++;
        for (auto &&v : key.second)
            if (auto t = dynamic_cast<Instruction *>(v); degrees.count(t))
                degrees[t]++, edges[inst].push_back(t);
        ++it;
    }

    auto comp = [&](Instruction *x, Instruction *y) { return origin[x] > origin[y]; };
    std::vector<Instruction *> result;
    std::set<Instruction *, decltype(comp)> candidates{comp};
    for (auto &&[inst, deg] : degrees)
        if (deg == 0) candidates.insert(inst);
    auto dfs = [&](Instruction *inst, auto &&self) -> void {
        candidates.erase(inst);
        result.push_back(inst);
        for (auto &&v : edges[inst]) {
            degrees[v]--;
            if (degrees[v] == 0) candidates.insert(v);
        }
        if (!edges[inst].empty() && candidates.count(edges[inst][0])) self(edges[inst][0], self);
    };
    while (!candidates.empty()) dfs(*candidates.begin(), dfs);
    for (auto &&inst : result) {
        bb->instructions.erase(inst->node);
        bb->insert(bb->beginner_end(), inst);
    }
}

void globalVariableNumbering(const Function *func) {
    std::vector<BasicBlock *> sorted_bbs{func->bbs.begin(), func->bbs.end()};
    std::sort(sorted_bbs.begin(), sorted_bbs.end(),
              [](auto &&x, auto &&y) { return x->dom_depth < y->dom_depth; });

    for (auto &&bb : sorted_bbs) globalVariableNumbering(bb);
    variables.clear();
}
}  // namespace mir
