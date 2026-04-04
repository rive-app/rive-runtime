/*
 * Copyright 2023 Rive
 */

#ifndef _RIVE_ENUM_BITSET_HPP_
#define _RIVE_ENUM_BITSET_HPP_

#include <cassert>
#include <type_traits>

namespace rive::enums
{

namespace internal
{
// Helper struct to test whether a type is a scoped enum ("enum class" or
// "enum struct")
template <typename Enum, typename = Enum>
struct IsScopedEnumImpl : std::is_enum<Enum>
{};

// non-scoped enums are implicitly convertible to integers, which means the
// unary + operator works for them. If the unary + operation works, then the
// given type is actually a scoped enum.
template <typename Enum>
struct IsScopedEnumImpl<Enum, decltype(Enum(+std::declval<Enum>()))>
    : std::false_type
{};

template <typename Enum>
constexpr bool is_scoped_enum = IsScopedEnumImpl<Enum>::value;

// Helper struct to test whether a type is a "flag enum" (which would be a
// scoped enum that has either a "none", "None", or "NONE" element that equals
// 0.
template <typename Enum, typename NoneType = Enum>
struct IsFlagEnumImpl : std::false_type
{};

template <typename Enum>
struct IsFlagEnumImpl<Enum, decltype(Enum::none)>
    : std::integral_constant<bool, is_scoped_enum<Enum>&& int(Enum::none) == 0>
{};

template <typename Enum>
struct IsFlagEnumImpl<Enum, decltype(Enum::None)>
    : std::integral_constant<bool, is_scoped_enum<Enum>&& int(Enum::None) == 0>
{};

template <typename Enum>
struct IsFlagEnumImpl<Enum, decltype(Enum::NONE)>
    : std::integral_constant<bool, is_scoped_enum<Enum>&& int(Enum::NONE) == 0>
{};

template <typename Enum>
constexpr bool is_flag_enum = IsFlagEnumImpl<Enum>::value;

#define RIVE_ENABLE_IF_IS_SCOPED_ENUM(Enum)                                    \
    template <typename Enum,                                                   \
              std::enable_if_t<enums::internal::is_scoped_enum<Enum>, int> =   \
                  0>

#define RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)                                      \
    template <typename Enum,                                                   \
              std::enable_if_t<enums::internal::is_flag_enum<Enum>, int> = 0>

} // namespace internal

RIVE_ENABLE_IF_IS_SCOPED_ENUM(Enum)
constexpr std::underlying_type_t<Enum> underlying_value(Enum v) noexcept
{
    return std::underlying_type_t<Enum>(v);
}

RIVE_ENABLE_IF_IS_SCOPED_ENUM(Enum)
constexpr Enum incr(Enum v) noexcept { return Enum(underlying_value(v) + 1); }

RIVE_ENABLE_IF_IS_SCOPED_ENUM(Enum)
constexpr Enum decr(Enum v) noexcept { return Enum(underlying_value(v) - 1); }

} // namespace rive::enums

// The operators need to live in the base `rive` namespace so they're
// discoverable from anywhere in the codebase
namespace rive
{

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum operator&(Enum a, Enum b) noexcept
{
    return Enum(enums::underlying_value(a) & enums::underlying_value(b));
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum operator^(Enum a, Enum b) noexcept
{
    return Enum(enums::underlying_value(a) ^ enums::underlying_value(b));
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum operator|(Enum a, Enum b) noexcept
{
    return Enum(enums::underlying_value(a) | enums::underlying_value(b));
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum operator~(Enum a) noexcept
{
    return Enum(~enums::underlying_value(a));
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum& operator&=(Enum& a, Enum b) noexcept
{
    a = (a & b);
    return a;
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum& operator^=(Enum& a, Enum b) noexcept
{
    a = (a ^ b);
    return a;
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
inline constexpr Enum& operator|=(Enum& a, Enum b) noexcept
{
    a = (a | b);
    return a;
}
} // namespace rive

namespace rive::enums
{
RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool is_single_flag(Enum flags) noexcept
{
    auto u = underlying_value(flags);
    return u != 0 && (u & (u - 1)) == 0;
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool is_flag_set(Enum flags, Enum testFlag) noexcept
{
    assert(is_single_flag(testFlag));
    return (flags & testFlag) != Enum(0);
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool any_flag_set(Enum flags) noexcept { return flags != Enum(0); }

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool any_flag_set(Enum flags, Enum mask) noexcept
{
    return any_flag_set(flags & mask);
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool all_flags_set(Enum flags, Enum mask) noexcept
{
    return (flags & mask) == mask;
}

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool no_flags_set(Enum flags) noexcept { return flags == Enum(0); }

RIVE_ENABLE_IF_IS_FLAG_ENUM(Enum)
constexpr bool no_flags_set(Enum flags, Enum mask) noexcept
{
    return no_flags_set(flags & mask);
}
} // namespace rive::enums

#endif

#undef RIVE_ENABLE_IF_IS_FLAG_ENUM
#undef RIVE_ENABLE_IF_IS_SCOPED_ENUM