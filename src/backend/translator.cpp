//
// Created by toby on 2023/11/5.
//

#include "translator.h"

namespace backend {
    static inline void flatten(mir::Literal *literal, std::vector<int> &result) {
        if (auto v = dynamic_cast<mir::IntegerLiteral *>(literal)) {
            return result.push_back(v->value);
        }
        auto arr = dynamic_cast<mir::ArrayLiteral *>(literal);
        assert(arr);
        for (auto ele: arr->values) {
            flatten(ele, result);
        }
    }

    static inline std::vector<int> flatten(mir::Literal *literal) {
        std::vector<int> result;
        flatten(literal, result);
        return result;
    }
}

namespace backend {
    void Translator::translateFunction(mir::Function *mirFunction) {
        //TODO
    }

    void Translator::translateBasicBlock(mir::BasicBlock *mirBlock) {
        //TODO
    }

    void Translator::translateInstruction(mir::Instruction *mirInst) {
        //TODO
    }

    void Translator::translateGlobalVar(mir::GlobalVar *mirVar) {
        mips::rGlobalVar result;
        std::string name(mirVar->getName());
        name[0] = '$';
        if (mirVar->init == nullptr)
            result = new mips::GlobalVar{name, false, false, false,
                                         (unsigned) mirVar->getType()->size(), {}};
        else if (auto str = dynamic_cast<mir::StringLiteral *>(mirVar->init))
            result = new mips::GlobalVar{name, true, true, true,
                                         (unsigned) mirVar->getType()->size(), str->value};
        else
            result = new mips::GlobalVar{name, true, false, false,
                                         (unsigned) mirVar->getType()->size(), flatten(mirVar->init)};
        gMap[mirVar] = result;
        mipsModule->globalVars.push_back(mips::pGlobalVar(result));
    }

    void Translator::translateOperand(mir::Value *mirValue) {
        //TODO
    }
}
