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

        constexpr static_string(const static_string<N> &) noexcept = default;

        constexpr static_string(static_string<N> &&) noexcept = delete;

        constexpr char operator[](size_t i) const noexcept { return data[i]; }

        constexpr char &operator[](size_t i) noexcept { return data[i]; }

        constexpr explicit operator std::string_view() const noexcept { return data; }

        [[nodiscard]] static constexpr bool empty() noexcept { return N == 0; }

        [[nodiscard]] static constexpr size_t length() noexcept { return N; }
    };

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

    template<typename Enum, Enum e>
    constexpr static auto get_value_name() noexcept {
        return STATIC_STRING((enum_to_string_helper<Enum, e>().second));
    }

    template<typename Enum, underlying_type_t<Enum> idx = 0>
    constexpr static size_t enum_count() noexcept {
        if constexpr (get_value_name<Enum, static_cast<Enum>(idx)>().empty()) return idx;
        else return enum_count<Enum, idx + 1>();
    }

    template<typename Enum, size_t N = enum_count<Enum>()>
    struct enum_to_string_struct {
        constexpr static auto last = get_value_name<Enum, static_cast<Enum>(N - 1)>();
        std::string_view enum_string[N];

        constexpr enum_to_string_struct() noexcept {
            if constexpr (N > 1) {
                const enum_to_string_struct<Enum, N - 1> prev{};
                for (size_t i = 0; i < N - 1; i++) enum_string[i] = prev.enum_string[i];
                enum_string[N - 1] = std::string_view(last);
            } else if constexpr (N == 1) {
                enum_string[0] = std::string_view(last);
            }
        }
    };

    template<typename Enum>
    constexpr enum_to_string_struct<Enum> enum_to_string_impl{};

#undef STATIC_STRING
}

namespace magic_enum {
    template<typename Enum>
    static constexpr auto enum_to_string(underlying_type_t<Enum> v) noexcept {
        return detail::enum_to_string_impl<Enum>.enum_string[v];
    }

    template<typename Enum>
    static constexpr auto enum_to_string(Enum v) noexcept {
        return enum_to_string<Enum>(static_cast<underlying_type_t<Enum>>(v));
    }

    template<typename T>
    static auto enum_to_string_lower(T v) noexcept {
        std::string str{enum_to_string(v)};
        for (auto &c: str)
            c = static_cast<char>(std::tolower(c));
        return str;
    }

    namespace static_test_only {
        enum class Foo {
            Zero, One, Two, Three
        };

        using namespace std::literals::string_view_literals;
        static_assert(enum_to_string<Foo>(2) == "Two"sv);
        static_assert(enum_to_string(Foo::Three) == "Three"sv);
    }
}

#endif //COMPILER_ENUM_H
