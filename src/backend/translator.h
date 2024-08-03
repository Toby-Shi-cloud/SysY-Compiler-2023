//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_BACKEND_TRANSLATOR_H
#define COMPILER_BACKEND_TRANSLATOR_H

#include "backend/component.h"
#include "mir.h"

namespace backend {
class TranslatorBase {
 protected:
    mir::Manager *mirManager;
    rModule assemblyModule;
    std::unordered_map<const mir::Function *, rFunction> fMap;
    std::unordered_map<const mir::BasicBlock *, rBlock> bMap;
    std::unordered_map<const mir::GlobalVar *, rGlobalVar> gMap;
    std::unordered_map<const mir::Value *, rOperand> oMap;
    std::unordered_map<rOperand, const mir::Value *> rMap;
    rFunction curFunc = nullptr;
    rBlock curBlock = nullptr;

    void put(const mir::Value *value, rOperand operand) {
        if (oMap.count(value)) {
            auto old = dynamic_cast<rRegister>(oMap[value]);
            auto reg = dynamic_cast<rRegister>(operand);
            assert(old && reg);
            old->swapTo(reg);
            rMap.erase(old);
        }
        oMap[value] = operand;
        rMap[operand] = value;
    }

    rRegister getRegister(const mir::Value *mirValue) {
        return dynamic_cast<rRegister>(translateOperand(mirValue));
    }

    rAddress getAddress(const mir::Value *mirValue) {
        auto ptr = translateOperand(mirValue);
        if (auto addr = dynamic_cast<rAddress>(ptr)) return addr;
        if (auto reg = dynamic_cast<rRegister>(ptr)) return curFunc->newAddress(reg, 0);
        if (auto label = dynamic_cast<rLabel>(ptr))
            return curFunc->newAddress(getZeroRegister(), 0, label);
        return nullptr;
    }

    void translateFunctions() {
        for (auto func : mirManager->functions) {
            translateFunction(func);
        }
    }

    void translateGlobalVars() {
        for (auto var : mirManager->globalVars) {
            translateGlobalVar(var);
        }
    }

    virtual rOperand translateOperand(const mir::Value *mirValue) = 0;
    virtual void translateFunction(const mir::Function *mirFunction) = 0;
    virtual void translateGlobalVar(const mir::GlobalVar *mirGlobalVar) = 0;
    virtual rRegister getZeroRegister() const = 0;

 public:
    explicit TranslatorBase(mir::Manager *mirManager, rModule assemblyModule)
        : mirManager(mirManager), assemblyModule(assemblyModule) {}

    virtual void translate() {
        translateGlobalVars();
        translateFunctions();
    }
};
}  // namespace backend

#endif  // COMPILER_BACKEND_TRANSLATOR_H
