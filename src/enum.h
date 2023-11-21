//
// Created by toby on 2023/10/31.
//

#ifndef COMPILER_ENUM_H
#define COMPILER_ENUM_H

#include <type_traits>
#include <string_view>

namespace magic_enum {
    using std::underlying_type_t;
}

namespace magic_enum::detail {
    template<size_t N>
    struct static_string {
        char data[N + 1]{};

        explicit constexpr static_string(const char (&str)[N + 1]) noexcept: data(str) {}

        explicit constexpr static_string(std::string_view sv) noexcept {
            for (size_t i = 0; i < N; i++) data[i] = sv[i];
            data[N] = '\0';
        }

        template<typename Func>
        explicit constexpr static_string(std::string_view sv, Func f) noexcept {
            for (size_t i = 0; i < N; i++) data[i] = f(sv[i]);
            data[N] = '\0';
        }

        template<typename Func>
        explicit constexpr static_string(static_string ss, Func f) noexcept {
            for (size_t i = 0; i < N; i++) data[i] = f(ss[i]);
            data[N] = '\0';
        }

        constexpr char operator[](size_t i) const noexcept { return data[i]; }

        constexpr char &operator[](size_t i) noexcept { return data[i]; }

        constexpr explicit operator std::string_view() const noexcept { return data; }

        [[nodiscard]] static constexpr bool empty() noexcept { return N == 0; }

        [[nodiscard]] static constexpr size_t length() noexcept { return N; }
    };

    constexpr int tolower(int ch) {
        return ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch;
    }

    constexpr int toupper(int ch) {
        return ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
    }

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
        constexpr auto enum_value = enum_value_.substr(enum_value_.find('e') + 4);
        if constexpr (enum_value.find('(') != std::string_view::npos || enum_value[0] >= '0' && enum_value[0] <= '9')
            return std::make_pair(enum_name, ""sv);
        else if constexpr (enum_value.find(':') != std::string_view::npos)
            return std::make_pair(enum_name, enum_value.substr(enum_value.rfind(':') + 1));
        return std::make_pair(enum_name, enum_value);
    }

#define STATIC_STRING(sv) static_string<sv.length()>(sv)

    template<typename Enum>
    constexpr auto enum_name = STATIC_STRING((enum_to_string_helper<Enum, (Enum) 0>().first));

    template<typename Enum, Enum e>
    constexpr auto enum_value = STATIC_STRING((enum_to_string_helper<Enum, e>().second));

#undef STATIC_STRING
#define STATIC_STRING(sv, f) static_string<sv.length()>(sv, f)

    template<typename Enum, Enum e>
    constexpr auto enum_value_lower = STATIC_STRING((enum_value<Enum, e>), tolower);

    template<typename Enum, Enum e>
    constexpr auto enum_value_upper = STATIC_STRING((enum_value<Enum, e>), toupper);

#undef STATIC_STRING

    template<typename Enum, underlying_type_t<Enum> e, int,
            typename = std::enable_if_t<enum_value<Enum, (Enum) e>.empty()>>
    static constexpr std::string_view enum_to_string_impl(underlying_type_t<Enum>) noexcept {
        return "Unknown";
    }

    template<typename Enum, underlying_type_t<Enum> e, int ty,
            typename = std::enable_if_t<!enum_value<Enum, (Enum) e>.empty()>>
    static constexpr auto enum_to_string_impl(underlying_type_t<Enum> v) noexcept {
        static_assert(ty >= 0 && ty <= 2, "ty must be 0, 1 or 2.");
        if (v == e) {
            if constexpr (ty == 0) return std::string_view(enum_value<Enum, (Enum) e>);
            else if constexpr (ty == 1) return std::string_view(enum_value_lower<Enum, (Enum) e>);
            else if constexpr (ty == 2) return std::string_view(enum_value_upper<Enum, (Enum) e>);
        }
        return enum_to_string_impl<Enum, e + 1, ty>(v);
    }
}

namespace magic_enum {
    template<typename Enum, int ty = 0>
    static constexpr auto enum_to_string(underlying_type_t<Enum> v) noexcept {
        return detail::enum_to_string_impl<Enum, 0, ty>(v);
    }

    template<typename Enum, int ty = 0>
    static constexpr auto enum_to_string(Enum v) noexcept {
        return detail::enum_to_string_impl<Enum, 0, ty>((underlying_type_t<Enum>) v);
    }

    template<typename Enum>
    static constexpr auto enum_to_string_lower(underlying_type_t<Enum> v) noexcept {
        return enum_to_string<Enum, 1>(v);
    }

    template<typename Enum>
    static constexpr auto enum_to_string_lower(Enum v) noexcept {
        return enum_to_string<Enum, 1>(v);
    }

    template<typename Enum>
    static constexpr auto enum_to_string_upper(underlying_type_t<Enum> v) noexcept {
        return enum_to_string<Enum, 2>(v);
    }

    template<typename Enum>
    static constexpr auto enum_to_string_upper(Enum v) noexcept {
        return enum_to_string<Enum, 2>(v);
    }

    namespace static_test_only {
        enum class Foo {
            Zero, One, Two, Three
        };

        static_assert(std::string_view(detail::enum_name<Foo>) == "magic_enum::static_test_only::Foo");
        static_assert(std::string_view(detail::enum_value<Foo, (Foo) 1>) == "One");
        static_assert(enum_to_string<Foo>(2) == "Two");
        static_assert(enum_to_string(Foo::Three) == "Three");
        static_assert(enum_to_string<Foo>(4) == "Unknown");
        static_assert(enum_to_string_lower(Foo::Two) == "two");
        static_assert(enum_to_string_upper<Foo>(3) == "THREE");
    }
}

#endif //COMPILER_ENUM_H
