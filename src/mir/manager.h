//
// Created by toby2 on 2023/10/13.
//

#ifndef COMPILER_MANAGER_H
#define COMPILER_MANAGER_H

#include "derived_value.h"

namespace mir {
    struct Manager {
        std::vector<Function *> functions;
        std::vector<GlobalVar *> globalVars;
        std::unordered_set<Literal *> literalPool;

        ~Manager() {
            for (auto function : functions)
                delete function;
            for (auto globalVar : globalVars)
                delete globalVar;
            for (auto literal : literalPool)
                delete literal;
        }
    };
}

#endif //COMPILER_MANAGER_H
