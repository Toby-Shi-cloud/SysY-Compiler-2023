//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_MIPS_PRINTER_H
#define COMPILER_MIPS_PRINTER_H

#include "mips/instruction.h"

namespace backend::mips {
inline std::ostream &operator<<(std::ostream &os, const GlobalVar &var) {
    if (var.inExtern) return os << "\t.extern\t" << var.label << ", " << var.size << "\n";
    os << var.label << ":" << "\n";
    if (!var.isInit)
        os << "\t.space\t" << var.size << "\n";
    else if (var.isString) {
        os << "\t.asciiz\t\"";
        for (char ch : std::get<std::string>(var.elements)) {
            if (ch == '\n')
                os << "\\n";
            else
                os << ch;
        }
        os << "\"" << std::endl;
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
    os << "\t.data" << "\n";
    for (auto &var : module.globalVars)
        if (!var->isString) os << *var;
    for (auto &var : module.globalVars)
        if (var->isString) os << *var;
    os << "\n"
       << "\t.text" << "\n";
    os << *module.main;
    for (auto &func : module.functions) os << std::endl << *func;
    return os;
}

void inline_printer(std::ostream &os, const Module &module);
}  // namespace backend::mips

#endif  // COMPILER_MIPS_PRINTER_H
