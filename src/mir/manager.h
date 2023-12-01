//
// Created by toby2 on 2023/10/13.
//

#ifndef COMPILER_MIR_MANAGER_H
#define COMPILER_MIR_MANAGER_H

#include <functional>
#include <unordered_map>
#include "derived_value.h"

namespace mir {
    struct Manager {
        std::vector<Function *> functions;
        std::vector<GlobalVar *> globalVars;
        std::unordered_map<std::string, GlobalVar *> stringPool;

        ~Manager() {
            for (auto function: functions)
                delete function;
            for (auto globalVar: globalVars)
                delete globalVar;
            // stringPool is a subset of globalVars
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

        void allocName() const {
            std::unordered_map<std::string, int> names;
            for (auto globalVar: globalVars)
                if (auto &cnt = names[globalVar->getName()]; cnt++)
                    globalVar->setName(globalVar->getName() + "." + std::to_string(cnt));
            for (auto function: functions)
                function->allocName();
        }

        template<typename Func>
        void for_each_func(Func &&f) {
            for (auto func: functions)
                std::invoke(f, func);
        }

        template<typename Func>
        void for_each_glob(Func &&f) {
            for (auto glob: globalVars)
                std::invoke(f, glob);
        }

        void optimize();

        void output(std::ostream &os) const {
            for (auto globalVar: globalVars)
                os << globalVar << std::endl;
            os << std::endl;
            os << "declare dso_local i32 @getint()" << std::endl;
            os << "declare dso_local void @putint(i32)" << std::endl;
            os << "declare dso_local void @putch(i32)" << std::endl;
            os << "declare dso_local void @putstr(ptr)" << std::endl;
            os << std::endl;
            for (auto function: functions)
                os << function << std::endl;
        }
    };
}

#endif //COMPILER_MIR_MANAGER_H
