#ifndef _RIVE_TEXT_MODIFIER_FLAGS_HPP_
#define _RIVE_TEXT_MODIFIER_FLAGS_HPP_

#include <cstdint>

namespace rive
{

enum class TextModifierFlags : uint8_t
{
    modifyOrigin = 1 << 0,
    modifyTranslation = 1 << 2,
    modifyRotation = 1 << 3,
    modifyScale = 1 << 4,
    modifyOpacity = 1 << 5,
    invertOpacity = 1 << 6
};

inline constexpr TextModifierFlags operator&(TextModifierFlags lhs, TextModifierFlags rhs)
{
    return static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) &
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));
}

inline constexpr TextModifierFlags operator^(TextModifierFlags lhs, TextModifierFlags rhs)
{
    return static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) ^
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));
}

inline constexpr TextModifierFlags operator|(TextModifierFlags lhs, TextModifierFlags rhs)
{
    return static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) |
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));
}

inline constexpr TextModifierFlags operator~(TextModifierFlags rhs)
{
    return static_cast<TextModifierFlags>(
        ~static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));
}

inline TextModifierFlags& operator|=(TextModifierFlags& lhs, TextModifierFlags rhs)
{
    lhs = static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) |
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));

    return lhs;
}

inline TextModifierFlags& operator&=(TextModifierFlags& lhs, TextModifierFlags rhs)
{
    lhs = static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) &
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));

    return lhs;
}

inline TextModifierFlags& operator^=(TextModifierFlags& lhs, TextModifierFlags rhs)
{
    lhs = static_cast<TextModifierFlags>(
        static_cast<std::underlying_type<TextModifierFlags>::type>(lhs) ^
        static_cast<std::underlying_type<TextModifierFlags>::type>(rhs));

    return lhs;
}
} // namespace rive
#endif