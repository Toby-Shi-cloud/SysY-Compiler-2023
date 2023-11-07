//
// Created by toby2 on 2023/10/13.
//

#ifndef COMPILER_MIR_MANAGER_H
#define COMPILER_MIR_MANAGER_H

#include <unordered_map>
#include "derived_value.h"

namespace mir {
    struct Manager {
        std::vector<Function *> functions;
        std::vector<GlobalVar *> globalVars;
        std::unordered_map<int, IntegerLiteral *> integerPool;
        std::unordered_map<std::string, GlobalVar *> stringPool;

        ~Manager() {
            for (auto function : functions)
                delete function;
            for (auto globalVar : globalVars)
                delete globalVar;
            for (auto literal : integerPool)
                delete literal.second;
            // stringPool is a subset of globalVars
        }

        IntegerLiteral *getIntegerLiteral(int value) {
            if (integerPool.find(value) == integerPool.end()) {
                integerPool[value] = new IntegerLiteral(value);
            }
            return integerPool[value];
        }

        GlobalVar *getStringLiteral(const std::string &value) {
            if (stringPool.find(value) == stringPool.end()) {
                auto str = new StringLiteral(value);
                auto var = new GlobalVar(str->getType(), str, true);
                var->setName("@.str");
                globalVars.push_back(var);
                stringPool[value] = var;
            }
            return stringPool[value];
        }

        void allocName() {
            std::unordered_map<std::string, int> names;
            for (auto globalVar : globalVars) {
                auto &cnt = names[globalVar->getName()];
                if (cnt++) {
                    globalVar->setName(globalVar->getName() + "." + std::to_string(cnt));
                }
            }
            for (auto function : functions) {
                function->allocName();
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

#endif //COMPILER_MIR_MANAGER_H
