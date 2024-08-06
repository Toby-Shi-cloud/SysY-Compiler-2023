//
// Created by toby on 2024/8/3.
//

#ifndef COMPILER_RISCV_OPERAND_H
#define COMPILER_RISCV_OPERAND_H

#include <memory>
#include <numeric>
#include <set>
#include <stack>
#include <type_traits>
#include <utility>
#include <vector>
#include "backend/operand.h"  // IWYU pragma: export
#include "riscv/alias.h"      // IWYU pragma: export

namespace backend::riscv {

struct Immediate : Operand {
    [[nodiscard]] virtual pImmediate clone() const = 0;
};

struct IntImmediate : Immediate {
    static inline std::stack<rIntImmediate> stack_vals = {};
    bool in_stack;
    int value;
    explicit IntImmediate(int value, bool in_stack = false) : value{value}, in_stack{in_stack} {
        if (in_stack) stack_vals.push(this);
    }
    std::ostream &output(std::ostream &os) const override { return os << value; }
    [[nodiscard]] pImmediate clone() const override {
        return std::make_unique<IntImmediate>(value, in_stack);
    }
};

struct SplitImmediate : Immediate {
    pImmediate origin;
    enum Partition { HI, LO } part;
    SplitImmediate(pImmediate origin, Partition part) : origin{std::move(origin)}, part{part} {}

    std::ostream &output(std::ostream &os) const override {
        using magic_enum::uppercase::operator<<;
        return os << "%" << part << "(" << origin << ")";
    }

    [[nodiscard]] pImmediate clone() const override {
        return std::make_unique<SplitImmediate>(origin->clone(), part);
    }
};

struct JoinImmediate : Immediate {
    rLabel label;
    std::vector<pIntImmediate> values;

    JoinImmediate(rLabel label, std::vector<pIntImmediate> values)
        : label{label}, values{std::move(values)} {}

    template <typename... Args>
    explicit JoinImmediate(rLabel label, Args &&...args) : label{label} {
        static_assert((std::is_same_v<Args, pIntImmediate &&> && ...),
                      "args should be rvalue reference of pIntImmediate");
        values.reserve(sizeof...(args));
        (values.push_back(std::move(args)), ...);
    }

    inline explicit JoinImmediate(const pImmediate &other);
    inline JoinImmediate(JoinImmediate &&o1, JoinImmediate &&o2);

    [[nodiscard]] int accumulate() const {
        return std::accumulate(values.begin(), values.end(), 0,
                               [](int acc, auto &&imm) { return acc + imm->value; });
    }

    std::ostream &output(std::ostream &os) const override {
        auto acc = accumulate();
        if (label != nullptr) {
            os << label;
            if (acc) os << '+' << acc;
        } else {
            os << acc;
        }
        return os;
    }

    [[nodiscard]] pImmediate clone() const override {
        std::vector<pIntImmediate> new_values;
        new_values.reserve(values.size());
        for (auto &&value : values) new_values.push_back(std::make_unique<IntImmediate>(*value));
        return std::make_unique<JoinImmediate>(label, std::move(new_values));
    }
};

inline pIntImmediate create_imm(int value) { return std::make_unique<IntImmediate>(value); }
inline pIntImmediate create_stack_imm(int value) {
    return std::make_unique<IntImmediate>(value, true);
}
inline pIntImmediate operator""_I(unsigned long long value) {
    return create_imm(static_cast<int>(value));
}
inline pIntImmediate operator""_IS(unsigned long long value) {
    return create_stack_imm(static_cast<int>(value));
}

inline pSplitImmediate create_imm(pImmediate imm, SplitImmediate::Partition part) {
    return std::make_unique<SplitImmediate>(std::move(imm), part);
}
inline auto create_imm_hi(pImmediate imm) { return create_imm(std::move(imm), SplitImmediate::HI); }
inline auto create_imm_lo(pImmediate imm) { return create_imm(std::move(imm), SplitImmediate::LO); }

inline pJoinImmediate create_imm(rLabel label) { return std::make_unique<JoinImmediate>(label); }
inline JoinImmediate::JoinImmediate(const pImmediate &other) : label{} {
    if (auto i = dynamic_cast<IntImmediate *>(other.get())) {
        values.push_back(std::make_unique<IntImmediate>(*i));
    } else if (auto j = dynamic_cast<JoinImmediate *>(other.get())) {
        auto new_u_ = j->clone();
        auto new_ = static_cast<JoinImmediate *>(new_u_.get());  // NOLINT
        label = new_->label;
        values = std::move(new_->values);
    } else {
        throw;
    }
}
inline JoinImmediate::JoinImmediate(JoinImmediate &&o1, JoinImmediate &&o2) {
    label = o1.label == nullptr ? o2.label : o1.label;
    values = std::move(o1.values);
    for (auto &&value : o2.values) values.push_back(std::move(value));
    decltype(o2.values)().swap(o2.values);
}
inline pJoinImmediate join_imm(const pImmediate &x, const pImmediate &y) {
    auto x1 = JoinImmediate(x);
    auto y1 = JoinImmediate(y);
    return std::make_unique<JoinImmediate>(std::move(x1), std::move(y1));
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

    [[nodiscard]] static rPhyRegister get(const std::string &name);

    template <typename Func>
    static inline auto gets(Func &&pred) -> decltype((bool)std::invoke(pred, rPhyRegister(nullptr)),
                                                     phy_set_t{});

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
    static auto gets(Func &&pred) -> decltype((bool)std::invoke(pred, get(0)), phy_set_t{}) {
        phy_set_t regs;
        for (auto &&reg : registers())
            if (std::invoke(pred, reg.get())) regs.insert(reg.get());
        return regs;
    }

    [[nodiscard]] bool isTemp() const final { return id >= 5 && id <= 7 || id >= 28 && id <= 31; }
    [[nodiscard]] bool isSaved() const final { return id >= 8 && id <= 9 || id >= 18 && id << 27; }
    bool isFloat() const override { return false; }

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
    bool isFloat() const override { return true; }

    template <typename Func>
    static auto gets(Func &&pred) -> decltype((bool)std::invoke(pred, get(0)), phy_set_t{}) {
        phy_set_t regs;
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
    -> decltype((bool)std::invoke(pred, rPhyRegister(nullptr)), phy_set_t{}) {
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
