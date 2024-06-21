//
// Created by toby on 2024/6/22.
//

#ifndef COMPILER_RISCV_REG_H
#define COMPILER_RISCV_REG_H

#include "../dbg.h"
#include "../mir/type.h"

namespace riscv {
    struct Register {
        bool fp;
        int id;

        constexpr Register(bool fp, int id) : fp(fp), id(id) {
            assert(0 <= id && id < 32);
        }

        constexpr static const char *x_name[] = {
            "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
            "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
            "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
            "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
        };
        constexpr static const char *f_name[] = {
            "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
            "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
            "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
            "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
        };

        [[nodiscard]] constexpr auto name() const {
            return fp ? f_name[id] : x_name[id];
        }

        [[nodiscard]] auto type() const {
            return fp ? mir::Type::getFloatType() : mir::Type::getI64Type();
        }
    };

    constexpr Register operator ""_R(const char *str, size_t) {
        if (str[0] == 'x' && isnumber(str[1])) {
            return {false, std::stoi(str + 1)};
        } else if (str[0] == 'f' && isnumber(str[1])) {
            return {true, std::stoi(str + 1)};
        } else if (auto x = std::find_if(Register::x_name, Register::x_name + 32,
                                         [=](auto n) { return std::strcmp(str, n) == 0; });
            x != Register::x_name + 32) {
            return {false, static_cast<int>(x - Register::x_name)};
        } else if (auto f = std::find_if(Register::f_name, Register::f_name + 32,
                                         [=](auto n) { return std::strcmp(str, n) == 0; });
            f != Register::f_name + 32) {
            return {true, static_cast<int>(f - Register::f_name)};
        } else {
            throw;
        }
    }

    inline std::ostream &operator<<(std::ostream &os, const Register &reg) {
        return os << reg.name();
    }
}

#endif //COMPILER_RISCV_REG_H
