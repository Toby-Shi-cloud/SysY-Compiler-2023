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
        auto &elements = std::get<std::vector<int>>(var.elements);
        os << "\t.word" << "\t";
        bool first = true;
        for (auto ele : elements) os << (first ? "" : ", ") << ele, first = false;
        os << std::endl;
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const Module &module) {
    // funcs
    os << *module.main << std::endl;
    for (auto &func : module.functions) os << *func << std::endl;
    // vars
    for (auto &var : module.globalVars) os << *var;
    return os;
}
}  // namespace backend::riscv

#endif  // COMPILER_RISCV_PRINTER_H
