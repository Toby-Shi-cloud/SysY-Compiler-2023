//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_OPERAND_H
#define COMPILER_MIPS_OPERAND_H

#include <variant>
#include <ostream>
#include "../dbg.h"

namespace mips {
    struct Label {
        std::string name;

        explicit Label(std::string name) : name(std::move(name)) {}

        friend inline std::ostream &operator<<(std::ostream &os, const Label &label) {
            return os << label.name;
        }
    };

    struct Immediate {
        int value;

        explicit Immediate(int value) : value(value) {}

        friend inline std::ostream &operator<<(std::ostream &os, const Immediate &imm) {
            return os << imm.value;
        }
    };

    struct PhyRegister {
        static constexpr const char *names[] = {
                "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
                "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
                "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
                "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
                "$(hi)", "$(lo)"
        };

        static inline const std::unordered_map<std::string, unsigned> name2id = []() {
            std::unordered_map<std::string, unsigned> name2id{};
            for (unsigned i = 0; i < 34; i++) name2id[names[i]] = i;
            return name2id;
        }();

        unsigned id;

        explicit PhyRegister(unsigned id) : id(id) { assert(id >= 0 && id < 34); }

        explicit PhyRegister(const std::string &name) : id(name2id.at(name)) {}

        friend inline std::ostream &operator<<(std::ostream &os, const PhyRegister &reg) {
            return os << names[reg.id];
        }
    };

    struct VirRegister {
        static inline unsigned counter = 0;
        unsigned id;

        explicit VirRegister() : id(counter++) {}

        friend inline std::ostream &operator<<(std::ostream &os, const VirRegister &reg) {
            return os << "$(vr" << reg.id << ")";
        }
    };

    using Register = std::variant<PhyRegister, VirRegister>;
    using Operand = std::variant<Label, Immediate, Register>;

#define OPERAND_TEMPLATE \
template<typename T, typename = std::enable_if_t<std::is_same_v<T, Register> || std::is_same_v<T, Operand>>>

    OPERAND_TEMPLATE
    inline std::ostream &operator<<(std::ostream &os, const T &var) {
        return std::visit([&os](auto &&arg) -> std::ostream & { return os << arg; }, var);
    }

#undef OPERAND_TEMPLATE
}

#endif //COMPILER_MIPS_OPERAND_H
