//
// Created by toby on 2023/12/8.
//

#include "../mips.h"

// inline printer is a special printer which will print some blocks several times (copy them) to avoid jump

namespace mips {
    struct InlineBlockContext {
        using content_t = std::variant<rInstruction, rBlock>;
        std::vector<content_t> contents;

        void push_back(rBlock block, rBlock label) {
            if (!contents.empty()) contents.pop_back();
            if (label) contents.emplace_back(label);
            for (auto &&sub: block->subBlocks)
                for (auto &&inst: *sub)
                    contents.emplace_back(inst.get());
        }
    };

    inline void inline_printer(std::ostream &os, const InlineBlockContext &ctx) {
        for (auto &&content: ctx.contents) {
            visit([&os](auto &&t) {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, rInstruction>)
                    os << "\t" << *t << "\n";
                else if (!t->label->name.empty())
                    os << t->label->name << ":\n";
            }, content);
        }
    }

    inline void inline_printer(std::ostream &os, const Function &func) {
        std::vector<InlineBlockContext> results;
        std::unordered_set<rBlock> labels;
        auto get_label = [&](rBlock block) -> rBlock {
            if (labels.count(block)) return nullptr;
            labels.insert(block);
            return block;
        };
        std::unordered_set<rBlock> inLoop;
        std::unordered_set<rBlock> visited;
        std::unordered_set<rBlock> inCurrent;
        auto process_loop = [&](rBlock _start) {
            InlineBlockContext result;
            rBlock _cur = _start;
            do {
                visited.insert(_cur);
                inLoop.insert(_cur);
                result.push_back(_cur, get_label(_cur));
                _cur = _cur->getJumpTo();
            } while (_cur != _start);
            return result;
        };
        auto dfs = [&](rBlock v) {
            inCurrent.clear();
            std::vector<rBlock> blocks;
            rBlock cur = v, loop_begin = nullptr;
            std::optional<InlineBlockContext> loop_result = std::nullopt;
            while (cur != nullptr) {
                if (inLoop.count(cur)) break;
                if (inCurrent.count(cur)) {
                    loop_begin = cur;
                    loop_result = process_loop(cur);
                    break;
                }
                inCurrent.insert(cur);
                blocks.push_back(cur);
                cur = cur->getJumpTo();
            }
            if (v == loop_begin) return;
            auto &&result = results.emplace_back();
            for (auto &&block: blocks) {
                if (block == loop_begin) break;
                result.push_back(block, get_label(block));
                visited.insert(block);
            }
            if (loop_result) {
                result.contents.pop_back();
                results.push_back(*loop_result);
            }
        };
        for (auto &&block: func) {
            if (visited.count(block.get())) continue;
            dfs(block.get());
        }
        for (auto &&b: labels)
            b->label->name.clear();
        for (auto &&[ctx]: results) {
            for (auto &&content: ctx) {
                if (std::holds_alternative<rBlock>(content)) continue;
                auto lbl = std::get<rInstruction>(content)->getJumpLabel();
                if (lbl == nullptr || !std::holds_alternative<rBlock>(lbl->parent)) continue;
                std::get<rBlock>(lbl->parent)->label->name = "><";
            }
        }
        size_t current_function_count = 0;
        for (auto &&[ctx]: results) {
            for (auto &&content: ctx) {
                if (std::holds_alternative<rInstruction>(content)) continue;
                auto block = std::get<rBlock>(content);
                if (block->label->name.empty()) continue;
                block->label->name = "_" + func.label->name + "." + std::to_string(++current_function_count);
            }
        }
        os << func.label->name << ":\n";
        for (auto &&ctx: results)
            inline_printer(os, ctx);
    }

    void inline_printer(std::ostream &os, const Module &module) {
        os << "\t.data" << "\n";
        for (auto &var: module.globalVars)
            if (!var->isString) os << *var;
        for (auto &var: module.globalVars)
            if (var->isString) os << *var;
        os << "\n" << "\t.text" << "\n";
        inline_printer(os, *module.main);
        for (auto &func: module.functions)
            inline_printer(os, *func);
    }
}
