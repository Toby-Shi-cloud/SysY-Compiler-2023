//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_BACKEND_OPERAND_H
#define COMPILER_BACKEND_OPERAND_H

#include <array>
#include <functional>
#include <ostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include "backend/alias.h"

namespace backend {
struct Operand {
    virtual inline std::ostream &output(std::ostream &os) const = 0;
    virtual ~Operand() = default;
};

struct Label : Operand {
    std::string name;
    std::variant<rBlock, rFunction, rGlobalVar> parent;

    template <typename T>
    explicit Label(std::string name, T parent) : name(std::move(name)), parent(parent) {}
    explicit Label(std::string name) : name(std::move(name)) {}
    std::ostream &output(std::ostream &os) const override { return os << name; }
};

struct Register : Operand {
    std::unordered_set<rInstructionBase> defUsers;
    std::unordered_set<rInstructionBase> useUsers;

    void swapDefTo(rRegister other, rSubBlock block = nullptr);
    void swapUseTo(rRegister other, rSubBlock block = nullptr);
    void swapTo(rRegister other, rSubBlock block = nullptr) {
        swapDefTo(other, block);
        swapUseTo(other, block);
    }
    void swapDefIn(rRegister other, rInstructionBase inst);
    void swapUseIn(rRegister other, rInstructionBase inst);
    [[nodiscard]] virtual bool isVirtual() const { return false; };
    [[nodiscard]] virtual bool isPhysical() const { return false; };
};

struct VirRegister : Register {
    size_t id;
    static inline size_t counter = 0;

    explicit VirRegister() : id(counter++) {}
    std::ostream &output(std::ostream &os) const override { return os << "$vr" << id << ""; }
    [[nodiscard]] bool isVirtual() const override { return true; };
};

template <typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
std::ostream &operator<<(std::ostream &os, const T &operand) {
    return operand.output(os);
}

template <typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
std::ostream &operator<<(std::ostream &os, const T *operand) {
    return operand->output(os);
}

template <typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
std::ostream &operator<<(std::ostream &os, const std::unique_ptr<T> &operand) {
    return operand->output(os);
}
}  // namespace backend

#endif  // COMPILER_BACKEND_OPERAND_H
