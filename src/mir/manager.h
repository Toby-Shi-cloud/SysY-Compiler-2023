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

        void allocName() {
            size_t counter = 0;
            for (auto globalVar : globalVars) {
                if (globalVar->unnamed) {
                    globalVar->setName("@.str" + (counter ? "." + std::to_string(counter) : ""));
                    ++counter;
                }
            }
            for (auto function : functions) {
                function->allocName();
            }
        }

        void cleanPool() {
            auto it = literalPool.begin();
            while (it != literalPool.end()) {
                auto literal = *it;
                if (!literal->isUsed() && literal->used == 0) {
                    it = literalPool.erase(it);
                    delete literal;
                } else {
                    it++;
                }
            }
        }

        void output(std::ostream &os) {
            allocName();
            for (auto globalVar : globalVars) {
                os << globalVar << std::endl;
            }
            os << std::endl;
            os << "declare dso_local i32 @getint()" << std::endl;
            os << "declare dso_local void @putint(i32)" << std::endl;
            os << "declare dso_local void @putch(i32)" << std::endl;
            os << "declare dso_local void @putstr(i8*)" << std::endl;
            os << std::endl;
            for (auto function : functions) {
                os << function << std::endl;
            }
        }
    };
}

#endif //COMPILER_MANAGER_H
