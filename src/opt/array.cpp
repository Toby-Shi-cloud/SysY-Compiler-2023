//
// Created by toby on 2023/12/16.
//

#include <functional>
#include "opt.h"

namespace mir {
    // an array can be split iff all its users index it with a constant.
    inline bool arrayCanSpilt(const Value *value) {
        for (auto &&user: value->users()) {
            if (dynamic_cast<Instruction::call *>(user)) {
                if (!value->getType()->isIntegerTy()) return false;
                continue;
            }
            if (auto gep = dynamic_cast<Instruction::getelementptr *>(user)) {
                for (auto i = 0; i < gep->getNumIndices(); i++)
                    if (!dynamic_cast<IntegerLiteral *>(gep->getIndexOperand(i)))
                        return false;
                if (!arrayCanSpilt(gep)) return false;
            }
        }
        return true;
    }

    template<typename T>
    inline void substituteArray(const Value *array, const std::vector<T> &new_array) {
        opt_infos.split_array()++;
        Interpreter interpreter;
        interpreter.map[array] = 0;
        auto dfs = [&](auto &&inst, auto &&self) -> void {
            if (auto gep = dynamic_cast<Instruction::getelementptr *>(inst)) {
                gep->interpret(interpreter);
                for (auto &&user: gep->users())
                    self(user, self);
                Value *new_value = new_array.at(interpreter.map.at(gep));
                substitute(gep, new_value);
            }
        };
        for (auto &&user: array->users())
            dfs(user, dfs);
    }

    auto spiltArray(Instruction::alloca_ *alloca_) {
        if (alloca_->getType()->isIntegerTy()) return std::next(alloca_->node);
        if (!arrayCanSpilt(alloca_)) return std::next(alloca_->node);
        auto bb = alloca_->parent;
        std::vector<Instruction::alloca_ *> new_alloca;
        new_alloca.reserve(alloca_->getType()->size() / 4);
        for (auto i = 0; i < alloca_->getType()->size() / 4; i++) {
            auto new_alloca_ = new Instruction::alloca_(Type::getI32Type());
            new_alloca.push_back(new_alloca_);
        }
        substituteArray(alloca_, new_alloca);
        for (auto &&new_alloca_: new_alloca)
            if (new_alloca_->isUsed())
                bb->insert(alloca_->node, new_alloca_);
            else delete new_alloca_;
        return bb->erase(alloca_);
    }

    void spiltArray(Manager &mgr, GlobalVar *global) {
        if (global->getType()->isIntegerTy()) return;
        if (global->getType()->isStringTy()) return;
        if (!arrayCanSpilt(global)) return;
        std::vector<GlobalVar *> new_global;
        new_global.reserve(global->getType()->size() / 4);
        auto dfs = [&](pType type, Literal *init, auto &&self) -> void {
            if (type->isIntegerTy()) {
                auto lit = dynamic_cast<IntegerLiteral *>(init);
                auto index = new_global.size();
                auto _new_val = new GlobalVar(
                        Type::getI32Type(), global->name.substr(1) + "." + std::to_string(index),
                        lit, global->isConstLVal());
                new_global.push_back(_new_val);
                return;
            }
            auto arr = dynamic_cast<ArrayLiteral *>(init);
            for (auto i = 0; i < type->getArraySize(); i++)
                self(type->getArrayBase(), arr ? arr->values[i] : nullptr, self);
        };
        dfs(global->getType(), global->init, dfs);
        substituteArray(global, new_global);
        for (auto &&_new_val: new_global)
            if (_new_val->isUsed())
                mgr.globalVars.push_back(_new_val);
            else delete _new_val;
    }

    void spiltArray(Manager &mgr) {
        auto backup = mgr.globalVars;
        for (auto &&var: backup)
            spiltArray(mgr, var);
        for (auto &&func: mgr.functions) {
            auto entry = func->bbs.front();
            for (auto it = entry->instructions.begin(); it != entry->instructions.end();)
                if (auto alloca_ = dynamic_cast<Instruction::alloca_ *>(*it))
                    it = spiltArray(alloca_);
                else break;
        }
    }

    void inlineGlobalVar(Manager &mgr) {
        auto main = *std::find_if(mgr.functions.begin(), mgr.functions.end(),
                                  std::function<bool(Function *)>(&Function::isMain));
        for (auto &&var: mgr.globalVars) {
            if (!var->getType()->isIntegerTy()) continue;
            auto users = var->users();
            if (std::any_of(users.begin(), users.end(), [&](auto user) {
                auto inst = dynamic_cast<Instruction *>(user);
                return inst->parent->parent != main;
            })) continue;
            auto alloca_ = new Instruction::alloca_(Type::getI32Type());
            main->bbs.front()->push_front(alloca_);
            main->bbs.front()->insert(main->bbs.front()->beginner_end(), new Instruction::store(
                    var->init ? var->init : new IntegerLiteral(0), alloca_));
            var->moveTo(alloca_);
        }
    }
}
