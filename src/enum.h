//
// Created by toby on 2023/10/31.
//

#ifndef COMPILER_ENUM_H
#define COMPILER_ENUM_H

#include <type_traits>
#include <string_view>

namespace magic_enum {
    using std::underlying_type_t;

    template<typename Enum, Enum e>
    static constexpr auto enum_to_string_helper() noexcept {
        using namespace std::string_view_literals;
        static_assert(std::is_enum_v<Enum>, "Enum must be an enum.");
        constexpr std::string_view sv = __PRETTY_FUNCTION__;
        constexpr auto lp = sv.find('[');
        constexpr auto rp = sv.find(']');
#if defined(__clang__)
        constexpr auto cm = sv.find(',');
#elif defined(__GNUC__) && !defined(__clang__)
        constexpr auto cm = sv.find(';');
#else
    #error "This compiler is not supported!"
#endif
        constexpr auto enum_name_ = sv.substr(lp + 1, cm - lp - 1);
        constexpr auto enum_name = enum_name_.substr(enum_name_.find("Enum") + 7);
        constexpr auto enum_value_ = sv.substr(cm + 1, rp - cm - 1);
        constexpr auto enum_value = enum_value_.substr(enum_value_.find("e") + 4);
        if constexpr (enum_value.find('(') != std::string_view::npos || enum_value[0] >= '0' && enum_value[0] <= '9')
            return std::make_pair(enum_name, ""sv);
        else if constexpr (enum_value.find(':') != std::string_view::npos)
            return std::make_pair(enum_name, enum_value.substr(enum_value.rfind(':') + 1));
        return std::make_pair(enum_name, enum_value);
    }

    template<typename Enum, Enum e>
    constexpr auto enum_name = enum_to_string_helper<Enum, e>().first;

    template<typename Enum, Enum e>
    constexpr auto enum_value = enum_to_string_helper<Enum, e>().second;

    template<typename Enum, underlying_type_t<Enum> e, typename = std::enable_if_t<
            enum_value<Enum, (Enum) e>.empty()>>
    static inline constexpr std::string_view enum_to_string_impl(underlying_type_t<Enum> v) noexcept {
        return "Unknown";
    }

    template<typename Enum, underlying_type_t<Enum> e, typename = std::enable_if_t<
            !enum_value<Enum, (Enum) e>.empty()>>
    static inline constexpr auto enum_to_string_impl(underlying_type_t<Enum> v) noexcept {
        if ((Enum) v == e) return enum_value<Enum, (Enum) e>;
        else return enum_to_string_impl<Enum, e + 1>(v);
    }

    template<typename Enum>
    static inline constexpr auto enum_to_string(underlying_type_t<Enum> v) noexcept {
        return enum_to_string_impl<Enum, 0>(v);
    }

    template<typename Enum>
    static inline constexpr auto enum_to_string(Enum v) noexcept {
        return enum_to_string_impl<Enum, 0>(v);
    }
}

#endif //COMPILER_ENUM_H
