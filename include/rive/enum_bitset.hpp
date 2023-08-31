/*
 * Copyright 2023 Rive
 */

#ifndef _RIVE_ENUM_BITSET_HPP_
#define _RIVE_ENUM_BITSET_HPP_

#include <type_traits>

namespace rive
{
// Wraps an enum containing a bitfield so that we can do implicit bool conversions.
template <typename T> class EnumBitset
{
public:
    using UnderlyingType = typename std::underlying_type<T>::type;
    constexpr EnumBitset() = default;
    constexpr EnumBitset(T value) : m_bits(static_cast<UnderlyingType>(value)) {}
    constexpr UnderlyingType bits() const { return m_bits; }
    constexpr operator T() const { return static_cast<T>(m_bits); }
    constexpr operator bool() const { return m_bits != 0; }

private:
    UnderlyingType m_bits = 0;
};
} // namespace rive

#define RIVE_MAKE_ENUM_BITSET(ENUM)                                                                \
    constexpr inline ::rive::EnumBitset<ENUM> operator&(::rive::EnumBitset<ENUM> lhs,              \
                                                        ::rive::EnumBitset<ENUM> rhs)              \
    {                                                                                              \
        return static_cast<ENUM>(lhs.bits() & rhs.bits());                                         \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator&(ENUM lhs, ::rive::EnumBitset<ENUM> rhs)    \
    {                                                                                              \
        return ::rive::EnumBitset<ENUM>(lhs) & rhs;                                                \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator&(::rive::EnumBitset<ENUM> lhs, ENUM rhs)    \
    {                                                                                              \
        return lhs & ::rive::EnumBitset<ENUM>(rhs);                                                \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator&(ENUM lhs, ENUM rhs)                        \
    {                                                                                              \
        return ::rive::EnumBitset<ENUM>(lhs) & ::rive::EnumBitset<ENUM>(rhs);                      \
    }                                                                                              \
                                                                                                   \
    constexpr inline ::rive::EnumBitset<ENUM> operator|(::rive::EnumBitset<ENUM> lhs,              \
                                                        ::rive::EnumBitset<ENUM> rhs)              \
    {                                                                                              \
        return static_cast<ENUM>(lhs.bits() | rhs.bits());                                         \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator|(ENUM lhs, ::rive::EnumBitset<ENUM> rhs)    \
    {                                                                                              \
        return ::rive::EnumBitset<ENUM>(lhs) | rhs;                                                \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator|(::rive::EnumBitset<ENUM> lhs, ENUM rhs)    \
    {                                                                                              \
        return lhs | ::rive::EnumBitset<ENUM>(rhs);                                                \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator|(ENUM lhs, ENUM rhs)                        \
    {                                                                                              \
        return ::rive::EnumBitset<ENUM>(lhs) | ::rive::EnumBitset<ENUM>(rhs);                      \
    }                                                                                              \
                                                                                                   \
    constexpr inline ::rive::EnumBitset<ENUM> operator~(::rive::EnumBitset<ENUM> rhs)              \
    {                                                                                              \
        return static_cast<ENUM>(~rhs.bits());                                                     \
    }                                                                                              \
    constexpr inline ::rive::EnumBitset<ENUM> operator~(ENUM rhs)                                  \
    {                                                                                              \
        return ~::rive::EnumBitset<ENUM>(rhs);                                                     \
    }                                                                                              \
                                                                                                   \
    inline ENUM& operator&=(ENUM& lhs, ::rive::EnumBitset<ENUM> rhs) { return lhs = lhs & rhs; }   \
    inline ENUM& operator&=(ENUM& lhs, ENUM rhs) { return lhs = lhs & rhs; }                       \
                                                                                                   \
    inline ENUM& operator|=(ENUM& lhs, ::rive::EnumBitset<ENUM> rhs) { return lhs = lhs | rhs; }   \
    inline ENUM& operator|=(ENUM& lhs, ENUM rhs) { return lhs = lhs | rhs; }

#endif
