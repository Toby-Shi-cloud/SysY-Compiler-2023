//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_RISCV_OPERAND_H
#define COMPILER_RISCV_OPERAND_H

#include <memory>
#include <utility>
#include <variant>
#include <vector>
#include "backend/operand.h"  // IWYU pragma: export
#include "riscv/alias.h"      // IWYU pragma: export
#include "util.h"

namespace backend::riscv {

struct Immediate : Operand {
    [[nodiscard]] virtual pImmediate clone() const = 0;
};

struct IntImmediate : Immediate {
    int value;
    explicit IntImmediate(int value) : value{value} {}
    std::ostream &output(std::ostream &os) const override { return os << value; }
    [[nodiscard]] pImmediate clone() const override {
        return std::make_unique<IntImmediate>(value);
    }
};

struct LabelImmediate : Immediate {
    std::variant<rLabel, pImmediate> label;
    enum Partition { HI, LO, FULL } part;
    LabelImmediate(rLabel label, Partition part) : label{label}, part{part} {}
    LabelImmediate(pImmediate label, Partition part) : label{std::move(label)}, part{part} {}

    std::ostream &output(std::ostream &os) const override {
        using magic_enum::uppercase::operator<<;
        os << "%" << part << "(";
        visit([&os](auto &&lbl) { os << lbl; }, label);
        return os << ")";
    }

    [[nodiscard]] pImmediate clone() const override {
        return visit(
            overloaded{
                [this](rLabel lbl) { return std::make_unique<LabelImmediate>(lbl, part); },
                [this](auto &imm) { return std::make_unique<LabelImmediate>(imm->clone(), part); }},
            label);
    }
};

struct JoinImmediate : Immediate {
    std::vector<pImmediate> values;
    explicit JoinImmediate(std::vector<pImmediate> values) : values{std::move(values)} {}

    [[nodiscard]] std::pair<rLabelImmediate, int> accumulate() const {
        rLabelImmediate label = nullptr;
        int acc = 0;
        for (auto &&imm : values) {
            if (auto lbl = dynamic_cast<rLabelImmediate>(imm.get())) {
                assert(label == nullptr);
                label = lbl;
            } else if (auto val = dynamic_cast<rIntImmediate>(imm.get())) {
                acc += val->value;
            } else if (auto join = dynamic_cast<rJoinImmediate>(imm.get())) {
                auto [l, a] = join->accumulate();
                if (l != nullptr) {
                    assert(label == nullptr);
                    label = l;
                }
                acc += a;
            } else {
                __builtin_unreachable();
            }
        }
        return {label, acc};
    }

    std::ostream &output(std::ostream &os) const override {
        auto [lbl, acc] = accumulate();
        if (lbl != nullptr) {
            os << lbl;
            if (acc) os << '+' << acc;
        } else {
            os << acc;
        }
        return os;
    }

    [[nodiscard]] pImmediate clone() const override {
        std::vector<pImmediate> new_values;
        new_values.reserve(values.size());
        for (auto &&value : values) new_values.push_back(value->clone());
        return std::make_unique<JoinImmediate>(std::move(new_values));
    }
};

inline pIntImmediate create_imm(int value) { return std::make_unique<IntImmediate>(value); }
inline pLabelImmediate create_imm(rLabel label, LabelImmediate::Partition part) {
    return std::make_unique<LabelImmediate>(label, part);
}
inline auto create_imm(rLabel label) { return create_imm(label, LabelImmediate::FULL); }
inline auto create_imm_hi(rLabel label) { return create_imm(label, LabelImmediate::HI); }
inline auto create_imm_lo(rLabel label) { return create_imm(label, LabelImmediate::LO); }
inline pIntImmediate operator""_I(unsigned long long value) {
    return create_imm(static_cast<int>(value));
}

inline pJoinImmediate join_imm(pImmediate x, pImmediate y) {
    if (auto join = dynamic_cast<rJoinImmediate>(x.get())) {
        join->values.push_back(std::move(y));
        (void)(join == x.release());  // release
        return pJoinImmediate{join};
    } else if (dynamic_cast<rJoinImmediate>(y.get())) {
        return join_imm(std::move(y), std::move(x));
    } else {
        std::vector<pImmediate> vec;
        vec.push_back(std::move(x)), vec.push_back(std::move(y));
        return std::make_unique<JoinImmediate>(std::move(vec));
    }
}

struct Address : Operand {
    rRegister base;
    pImmediate offset;
    Address(rRegister base, pImmediate offset) : base{base}, offset{std::move(offset)} {}
    std::ostream &output(std::ostream &os) const override {
        return os << offset << '(' << base << ')';
    }
};

struct PhyRegister : Register {
    unsigned id;

 protected:
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
    using xphy_set_t = std::set<rXPhyRegister, Comparator>;
    using fphy_set_t = std::set<rFPhyRegister, Comparator>;

    [[nodiscard]] static rPhyRegister get(const std::string &name);

    template <typename Func>
    static inline auto gets(Func &&pred)
        -> decltype((bool)std::invoke(pred, std::declval<PhyRegister>()), phy_set_t{});

    [[nodiscard]] bool isPhysical() const override { return true; };
    [[nodiscard]] virtual const char *name() const = 0;

    [[nodiscard]] bool isRet() const { return id >= 10 && id <= 11; }
    [[nodiscard]] bool isArg() const { return id >= 10 && id <= 17; }
    [[nodiscard]] virtual bool isTemp() const = 0;
    [[nodiscard]] virtual bool isSaved() const = 0;

    std::ostream &output(std::ostream &os) const override { return os << name(); }
};

struct XPhyRegister final : PhyRegister {
    constexpr static const char *names[] = {
        "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",  //
        "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",  //
        "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",  //
        "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",  //
    };
    static inline const std::unordered_map<std::string, unsigned> name2id = [] {
        std::unordered_map<std::string, unsigned> name2id{};
        for (unsigned i = 0; i < 32; i++) name2id[names[i]] = i;
        return name2id;
    }();

    [[nodiscard]] static rXPhyRegister get(unsigned id) { return registers()[id].get(); }
    [[nodiscard]] const char *name() const final { return names[id]; }

    template <typename Func>
    static auto gets(Func &&pred) -> decltype((bool)std::invoke(pred, get(0)), xphy_set_t{}) {
        xphy_set_t regs;
        for (auto &&reg : registers())
            if (std::invoke(pred, reg.get())) regs.insert(reg.get());
        return regs;
    }

    [[nodiscard]] bool isTemp() const final { return id >= 4 && id <= 7 || id >= 28 && id <= 31; }
    [[nodiscard]] bool isSaved() const final { return id >= 8 && id <= 9 || id >= 18 && id << 27; }

 private:
    friend PhyRegister;
    using PhyRegister::PhyRegister;
    static const std::array<pXPhyRegister, 32> &registers();
};

struct FPhyRegister final : PhyRegister {
    constexpr static const char *names[] = {
        "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",   //
        "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",   //
        "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",   //
        "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11",  //
    };
    static inline const std::unordered_map<std::string, unsigned> name2id = [] {
        std::unordered_map<std::string, unsigned> name2id{};
        for (unsigned i = 0; i < 32; i++) name2id[names[i]] = i;
        return name2id;
    }();

    [[nodiscard]] static rPhyRegister get(unsigned id) { return registers()[id].get(); }
    [[nodiscard]] const char *name() const final { return names[id]; }

    [[nodiscard]] bool isTemp() const final { return id >= 0 && id <= 7 || id >= 28 && id <= 31; }
    [[nodiscard]] bool isSaved() const final { return id >= 8 && id <= 9 || id >= 18 && id << 27; }

    template <typename Func>
    static auto gets(Func &&pred) -> decltype((bool)std::invoke(pred, get(0)), fphy_set_t{}) {
        fphy_set_t regs;
        for (auto &&reg : registers())
            if (std::invoke(pred, reg.get())) regs.insert(reg.get());
        return regs;
    }

 private:
    friend PhyRegister;
    using PhyRegister::PhyRegister;
    static const std::array<pFPhyRegister, 32> &registers();
};

inline const std::array<pXPhyRegister, 32> &XPhyRegister::registers() {
    static auto _registers = [] {
        std::array<pXPhyRegister, 32> _registers;
        for (unsigned i = 0; i < 32; i++) _registers[i] = pXPhyRegister(new XPhyRegister(i));
        return _registers;
    }();
    return _registers;
}

inline const std::array<pFPhyRegister, 32> &FPhyRegister::registers() {
    static auto _registers = [] {
        std::array<pFPhyRegister, 32> _registers;
        for (unsigned i = 0; i < 32; i++) _registers[i] = pFPhyRegister(new FPhyRegister(i));
        return _registers;
    }();
    return _registers;
}

inline rPhyRegister PhyRegister::get(const std::string &name) {
#define FIND(res, arr)                                           \
    auto(res) = std::find(std::begin(arr), std::end(arr), name); \
    (res) != std::end(arr)
    if (name[0] == 'x' && isdigit(name[1])) {
        return XPhyRegister::get(std::stoi(name.substr(1)));
    } else if (name[0] == 'f' && isdigit(name[1])) {
        return FPhyRegister::get(std::stoi(name.substr(1)));
    } else if (FIND(x, XPhyRegister::names)) {
        return XPhyRegister::get(x - XPhyRegister::names);
    } else if (FIND(f, FPhyRegister::names)) {
        return FPhyRegister::get(f - FPhyRegister::names);
    } else {
        return nullptr;
    }
#undef FIND
}

template <typename Func>
inline auto PhyRegister::gets(Func &&pred)
    -> decltype((bool)std::invoke(pred, std::declval<PhyRegister>()), phy_set_t{}) {
    phy_set_t regs;
    for (auto &&reg : XPhyRegister::registers())
        if (std::invoke(pred, reg.get())) regs.insert(reg.get());
    for (auto &&reg : FPhyRegister::registers())
        if (std::invoke(pred, reg.get())) regs.insert(reg.get());
    return regs;
}

inline rPhyRegister operator""_R(const char *str, size_t) { return PhyRegister::get(str); }

}  // namespace backend::riscv

#endif  // COMPILER_RISCV_OPERAND_H
