//
// Created by toby on 2023/11/5.
//

#ifndef COMPILER_MIPS_OPERAND_H
#define COMPILER_MIPS_OPERAND_H

#include <array>
#include <variant>
#include <ostream>
#include <unordered_set>
#include <unordered_map>
#include "alias.h"

namespace mips {
    struct Operand {
        virtual inline std::ostream &output(std::ostream &os) const = 0;

        virtual ~Operand() = default;
    };

    struct Label : Operand {
        std::string name;
        std::variant<rBlock, rFunction, rGlobalVar> parent;

        template<typename T>
        explicit Label(std::string name, T parent) : name(std::move(name)), parent(parent) {}

        std::ostream &output(std::ostream &os) const override {
            return os << name;
        }
    };

    struct Address : Operand {
        rRegister base;
        int offset;
        rLabel label;

        explicit Address(rRegister base, int offset, rLabel label = nullptr)
            : base(base), offset(offset), label(label) {}

        std::ostream &output(std::ostream &os) const override {
            return os << label << " + " << offset << "(" << base << ")";
        }
    };

    struct Immediate : Operand {
        int value;

        explicit Immediate(int value) : value(value) {}

        std::ostream &output(std::ostream &os) const override {
            return os << value;
        }
    };

    struct Register : Operand {
        std::unordered_set<rInstruction> defUsers;
        std::unordered_set<rInstruction> useUsers;

        void swapDefTo(rRegister other, rSubBlock block = nullptr);

        void swapUseTo(rRegister other, rSubBlock block = nullptr);

        void swapTo(rRegister other, rSubBlock block = nullptr) {
            swapDefTo(other, block);
            swapUseTo(other, block);
        }

        void swapDefIn(rRegister other, rInstruction inst);

        void swapUseIn(rRegister other, rInstruction inst);

        [[nodiscard]] bool isVirtual() const;

        [[nodiscard]] bool isPhysical() const;
    };

    struct PhyRegister : Register {
        static constexpr const char *names[] = {
            "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
            "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
            "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
            "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
            "$(hi)", "$(lo)"
        };

        static inline const std::unordered_map<std::string, unsigned> name2id = [] {
            std::unordered_map<std::string, unsigned> name2id{};
            for (unsigned i = 0; i < 34; i++) name2id[names[i]] = i;
            return name2id;
        }();

        unsigned id;

    private:
        static const std::array<pPhyRegister, 34> registers;

        explicit PhyRegister(unsigned id) : id(id) {}

    public:
        [[nodiscard]] static rPhyRegister get(unsigned id) {
            return registers[id].get();
        }

        [[nodiscard]] static rPhyRegister get(const std::string &name) {
            return registers[name2id.at(name)].get();
        }

        [[nodiscard]] bool isUniversal() const { return id >= 8 && id <= 25; }

        [[nodiscard]] bool isRet() const { return id >= 2 && id <= 3; }

        [[nodiscard]] bool isArg() const { return id >= 4 && id <= 7; }

        [[nodiscard]] bool isTemp() const { return id >= 8 && id <= 9 || id >= 24 && id <= 25; }

        [[nodiscard]] bool isSaved() const { return id >= 16 && id <= 23; }

        [[nodiscard]] bool isGp() const { return id == 28; }

        [[nodiscard]] bool isSp() const { return id == 29; }

        [[nodiscard]] bool isFp() const { return id == 30; }

        [[nodiscard]] bool isRa() const { return id == 31; }

        [[nodiscard]] bool isHi() const { return id == 32; }

        [[nodiscard]] bool isLo() const { return id == 33; }

        [[nodiscard]] bool isHiLo() const { return id >= 32; }

        std::ostream &output(std::ostream &os) const override {
            return os << names[id];
        }
    };

    inline const std::array<pPhyRegister, 34> PhyRegister::registers = [] {
        std::array<pPhyRegister, 34> registers;
        for (unsigned i = 0; i < 34; i++)
            registers[i] = pPhyRegister(new PhyRegister(i));
        return registers;
    }();

    struct VirRegister : Register {
        static inline unsigned counter = 0;
        unsigned id;

        explicit VirRegister() : id(counter++) {}

        std::ostream &output(std::ostream &os) const override {
            return os << "$vr" << id << "";
        }
    };

    inline bool Register::isVirtual() const {
        return dynamic_cast<const VirRegister *>(this) != nullptr;
    }

    inline bool Register::isPhysical() const {
        return dynamic_cast<const PhyRegister *>(this) != nullptr;
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
    std::ostream &operator<<(std::ostream &os, const T &operand) {
        return operand.output(os);
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
    std::ostream &operator<<(std::ostream &os, const T *operand) {
        return operand->output(os);
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Operand, T>>>
    std::ostream &operator<<(std::ostream &os, const std::unique_ptr<T> &operand) {
        return operand->output(os);
    }
}

#endif //COMPILER_MIPS_OPERAND_H
