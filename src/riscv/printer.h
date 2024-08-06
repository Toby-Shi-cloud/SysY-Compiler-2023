//
// Created by toby on 2024/8/5.
//

#ifndef COMPILER_RISCV_PRINTER_H
#define COMPILER_RISCV_PRINTER_H

#include <iostream>
#include "backend/component.h"
#include "riscv/alias.h"
#include "util.h"

namespace backend::riscv {
inline std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
    os << var.label << ":" << "\n";
    if (!var.isInit) {
        os << "\t.zero\t" << var.size << "\n";
    } else if (var.isString) {
        TODO("no string");
    } else {
        auto &elements = std::get<std::vector<std::pair<int, int>>>(var.elements);
        for (auto [val, times] : elements) {
            if (val == 0)
                os << "\t.zero\t" << times * 4 << "\n";
            else
                while (times--) os << "\t.word\t" << val << "\n";
        }
        os << std::endl;
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const Module &module) {
    // funcs
    os << "\t.text\n"
       << "\t.globl\tmain\n"
       << "\t.type\tmain, @function\n";
    os << *module.main << "\t.size\tmain, .-main\n\n";
    for (auto &func : module.functions) {
        const auto &name = func->label->name;
        os << "\t.globl\t" << name << "\n"
           << "\t.type\t" << name << ", @function\n";
        os << *func << "\t.size\t" << name << ", .-" << name << "\n\n";
    }
    // vars
    for (auto &var : module.globalVars) {
        const auto &name = var->label->name;
        os << "\t.globl\t" << name << "\n"
           << "\t.data\n"
           << "\t.align\t4\n"
           << "\t.type\t" << name << ", @object\n"
           << "\t.size\t" << name << ", " << var->size << "\n"
           << *var;
    }
    // memset0
    constexpr char memset0[] = (
#include "riscv/memset0.asm"
    );
    return os << memset0 << std::endl;
}
}  // namespace backend::riscv

#endif  // COMPILER_RISCV_PRINTER_H
