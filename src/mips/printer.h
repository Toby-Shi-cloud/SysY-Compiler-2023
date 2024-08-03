//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_MIPS_PRINTER_H
#define COMPILER_MIPS_PRINTER_H

#include "mips/instruction.h"

namespace backend::mips {
void inline_printer(std::ostream &os, const Module &module);
}

#endif  // COMPILER_MIPS_PRINTER_H
