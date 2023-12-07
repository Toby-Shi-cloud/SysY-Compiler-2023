//
// Created by toby on 2023/11/26.
//

#include <set>
#include <functional>
#include "opt.h"

using value_vector_t = std::vector<mir::Value *>;
using inst_vector_t = std::vector<mir::Instruction *>;
using operator_t = std::pair<mir::Instruction::InstrTy, value_vector_t>;

template<>
struct [[maybe_unused]] std::hash<value_vector_t> {
    static constexpr auto _hash = std::hash<mir::Value *>();

    size_t operator()(const value_vector_t &v) const noexcept {
        size_t ret = 0;
        for (const auto &e: v)
            ret = ret * 10007 + _hash(e);
        return ret;
    }
};

template<>
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

    inline void globalVariableNumbering(BasicBlock *bb) {
        std::unordered_map<Instruction *, inst_vector_t> edges;
        std::unordered_map<Instruction *, int> degrees;
        std::unordered_map<Instruction *, int> origin;
        int cnt = 0;

        //TODO: This is too simple...
        Value *last_memory_inst = bb;
        for (auto it = bb->beginner_end(); it != std::prev(bb->instructions.cend());) {
            auto inst = *it;
            auto values = inst->getOperands();
            bb->parent->allocName();
            if (inst->isCall() || inst->isMemoryAccess()) {
                values.push_back(last_memory_inst);
                last_memory_inst = inst;
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
            for (auto &&v: key.second)
                if (auto t = dynamic_cast<Instruction *>(v); degrees.count(t))
                    degrees[t]++, edges[inst].push_back(t);
            ++it;
        }

        auto comp = [&](Instruction *x, Instruction *y) { return origin[x] > origin[y]; };
        std::vector<Instruction *> result;
        std::set<Instruction *, decltype(comp)> candidates{comp};
        for (auto &&[inst, deg]: degrees)
            if (deg == 0) candidates.insert(inst);
        auto dfs = [&](Instruction *inst, auto &&self) -> void {
            candidates.erase(inst);
            result.push_back(inst);
            for (auto &&v: edges[inst]) {
                degrees[v]--;
                if (degrees[v] == 0)
                    candidates.insert(v);
            }
            if (!edges[inst].empty() && candidates.count(edges[inst][0]))
                self(edges[inst][0], self);
        };
        while (!candidates.empty())
            dfs(*candidates.begin(), dfs);
        for (auto &&inst: result) {
            bb->instructions.erase(inst->node);
            bb->insert(bb->beginner_end(), inst);
        }
    }

    void globalVariableNumbering(const Function *func) {
        std::vector<BasicBlock *> sorted_bbs{func->bbs.begin(), func->bbs.end()};
        std::sort(sorted_bbs.begin(), sorted_bbs.end(), [](auto &&x, auto &&y) {
            return x->dom_depth < y->dom_depth;
        });

        for (auto &&bb: sorted_bbs)
            globalVariableNumbering(bb);
        variables.clear();
    }
}
