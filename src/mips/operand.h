//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_OPERAND_H
#define COMPILER_MIPS_OPERAND_H

#include <array>
#include <functional>
#include <ostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include "backend/operand.h"
#include "mips/alias.h"

namespace backend::mips {

struct Immediate : Operand {
    int value;

    explicit Immediate(int value) : value(value) {}
    std::ostream &output(std::ostream &os) const override { return os << value; }
    [[nodiscard]] virtual pImmediate clone() const { return std::make_unique<Immediate>(value); }
    [[nodiscard]] virtual bool isDyn() const { return false; }  // 兼容实现
};

struct DynImmediate : Immediate {
    const int *base;

    explicit DynImmediate(int value, const int *base) : Immediate(value), base(base) {}
    std::ostream &output(std::ostream &os) const override { return os << value + *base; }
    [[nodiscard]] pImmediate clone() const override {
        return std::make_unique<DynImmediate>(value, base);
    }
    [[nodiscard]] bool isDyn() const override { return true; }
};

struct Address : Operand {
    rRegister base;
    pImmediate offset;
    rLabel label;

    explicit Address(rRegister base, int offset, rLabel label = nullptr)
        : base(base), offset(new Immediate(offset)), label(label) {}
    explicit Address(rRegister base, int offset, const int *immBase, rLabel label = nullptr)
        : base(base), offset(new DynImmediate(offset, immBase)), label(label) {}
    explicit Address(rRegister base, pImmediate offset, rLabel label = nullptr)
        : base(base), offset(std::move(offset)), label(label) {}

    std::ostream &output(std::ostream &os) const override {
        label->output(os);
        os << " + ";
        offset->output(os);
        os << "(";
        base->output(os);
        return os << ")";
    }
};

struct PhyRegister : Register {
    static constexpr const char *names[] = {
        "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",  //
        "$t0",   "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",  //
        "$s0",   "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",  //
        "$t8",   "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",  //
        "HI",    "LO"                                              //
    };

    static inline const std::unordered_map<std::string, unsigned> name2id = [] {
        std::unordered_map<std::string, unsigned> name2id{};
        for (unsigned i = 0; i < 34; i++) name2id[names[i]] = i;
        return name2id;
    }();

    unsigned id;

 private:
    static const std::array<pPhyRegister, 34> &registers();
    static const pPhyRegister &registers(unsigned id) { return registers()[id]; }

    explicit PhyRegister(unsigned id) : id(id) {}

    struct Comparator {
        bool operator()(rPhyRegister x, rPhyRegister y) const {
            if (x == nullptr) return y != nullptr;
            if (y == nullptr) return false;
            constexpr auto priority = [](rPhyRegister reg) {
                return reg->isTemp() << 0 | reg->isArg() << 1 | reg->isRet() << 2 |
                       reg->isSaved() << 3;
            };
            return priority(x) == priority(y) ? x->id < y->id : priority(x) < priority(y);
        }
    };

 public:
    using phy_set_t = std::set<rPhyRegister, Comparator>;

    [[nodiscard]] static rPhyRegister get(unsigned id) { return registers(id).get(); }

    [[nodiscard]] static rPhyRegister get(const std::string &name) {
        return registers(name2id.at(name)).get();
    }

    template <typename Func, typename = std::enable_if_t<std::is_convertible_v<
                                 std::invoke_result_t<Func, rPhyRegister>, bool>>>
    [[nodiscard]] static auto get(Func &&pred) {
        phy_set_t regs;
        for (auto &&reg : registers())
            if (std::invoke(pred, reg.get())) regs.insert(reg.get());
        return regs;
    }

    [[nodiscard]] bool isPhysical() const override { return true; };
    [[nodiscard]] bool isRet() const { return id >= 2 && id <= 3; }
    [[nodiscard]] bool isArg() const { return id >= 4 && id <= 7; }
    [[nodiscard]] bool isTemp() const { return id >= 8 && id <= 15 || id >= 24 && id <= 25; }
    [[nodiscard]] bool isSaved() const { return id >= 16 && id <= 23 || id == 30; }
    std::ostream &output(std::ostream &os) const override { return os << names[id]; }

    inline static const rPhyRegister HI = get("HI");  // NOLINT
    inline static const rPhyRegister LO = get("LO");  // NOLINT
};

inline const std::array<pPhyRegister, 34> &PhyRegister::registers() {
    static auto _registers = [] {
        std::array<pPhyRegister, 34> _registers;
        for (unsigned i = 0; i < 34; i++) _registers[i] = pPhyRegister(new PhyRegister(i));
        return _registers;
    }();
    return _registers;
}

}  // namespace backend::mips

#endif  // COMPILER_MIPS_OPERAND_H
