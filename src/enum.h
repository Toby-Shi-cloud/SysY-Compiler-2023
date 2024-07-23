//
// Created by toby on 2023/10/31.
//

#ifndef COMPILER_ENUM_H
#define COMPILER_ENUM_H

#include <magic_enum.hpp>

namespace magic_enum {
template <typename Enum>
auto operator<<(std::ostream &os, Enum e) -> typename std::enable_if_t<std::is_enum_v<Enum>, std::ostream &> {
    return os << magic_enum::enum_name(e);
}

namespace lowercase {
template <typename Enum>
auto operator<<(std::ostream &os, Enum e) -> typename std::enable_if_t<std::is_enum_v<Enum>, std::ostream &> {
    for (auto ch : magic_enum::enum_name(e)) os << (char)::tolower(ch);
    return os;
}
}  // namespace lowercase

namespace uppercase {
template <typename Enum>
auto operator<<(std::ostream &os, Enum e) -> typename std::enable_if_t<std::is_enum_v<Enum>, std::ostream &> {
    for (auto ch : magic_enum::enum_name(e)) os << (char)::toupper(ch);
    return os;
}
}  // namespace uppercase
}  // namespace magic_enum

#endif  // COMPILER_ENUM_H
