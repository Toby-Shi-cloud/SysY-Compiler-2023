//
// Created by toby on 2023/10/13.
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

        ~Manager() {
            return; //todo!
            for (auto function: functions)
                delete function;
            for (auto globalVar: globalVars)
                delete globalVar;
            // stringPool is a subset of globalVars
        }

        void allocName() const {
            std::unordered_map<std::string, int> names;
            for (auto globalVar: globalVars)
                if (auto &cnt = names[globalVar->name]; cnt++)
                    globalVar->name += "." + std::to_string(cnt);
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

        void clearUnused();

        void output(std::ostream &os) const {
            for (auto globalVar: globalVars)
                os << globalVar << std::endl;
            os << std::endl;
            for (auto func: Function::getLibrary()) {
                os << "declare dso_local " << func->retType << " " << func->name << "(";
                for (auto i = 0; i < func->getType()->getFunctionParamCount(); i++) {
                    if (i) os << ", ";
                    os << func->getType()->getFunctionParam(i);
                }
                os << ")" << std::endl;
            }
            os << std::endl;
            for (auto function: functions)
                os << function << std::endl;
            os << "declare void @llvm.memset.p0.i32(ptr, i8, i32, i1 immarg)" << std::endl;
        }
    };
}

#endif //COMPILER_MIR_MANAGER_H
