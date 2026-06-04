/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_MATH_TYPES_DEFINED_
#define _RIVE_MATH_TYPES_DEFINED_

#include "rive/rive_types.hpp"
#include <cmath>
#include <limits>
#include <string.h>

namespace rive
{

namespace math
{
constexpr float PI = 3.14159265f;
constexpr float SQRT2 = 1.41421356f;
constexpr float EPSILON =
    1.f / (1 << 12); // Common threshold for detecting values near zero.

RIVE_MAYBE_UNUSED inline bool nearly_zero(float a, float tolerance = EPSILON)
{
    assert(tolerance >= 0);
    return fabsf(a) <= tolerance;
}

RIVE_MAYBE_UNUSED inline bool nearly_equal(float a,
                                           float b,
                                           float tolerance = EPSILON)
{
    return nearly_zero(b - a, tolerance);
}

// Performs a floating point division with conformant IEEE 754 behavior for NaN
// and Inf.
//
//   Returns +/-Inf if b == 0.
//   Returns 0 if b == +/-Inf.
//   Returns NaN if a and b are both zero.
//   Returns NaN if a and b are both infinite.
//   Returns NaN a or b is NaN.
//
// Reference:
// https://stackoverflow.com/questions/42926763/the-behaviour-of-floating-point-division-by-zero
RIVE_MAYBE_UNUSED
#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("float-divide-by-zero"), always_inline))
#endif
inline static float
ieee_float_divide(float a, float b)
{
    static_assert(std::numeric_limits<float>::is_iec559,
                  "conformant IEEE 754 behavior for NaN and Inf is required");
    return a / b;
}

// Reinterprets the underlying bits of src as the given type.
template <typename Dst, typename Src> Dst bit_cast(const Src& src)
{
    static_assert(sizeof(Dst) == sizeof(Src), "sizes of both types must match");
    Dst dst;
    RIVE_INLINE_MEMCPY(&dst, &src, sizeof(Dst));
    return dst;
}

// Lossless cast function that asserts on overflow
template <typename T, typename U> T lossless_numeric_cast(U u)
{
    T t = static_cast<T>(u);
    assert(static_cast<U>(t) == u);

    // If there are signedness differences between integral types that needs
    // special handling.
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>)
    {
        if constexpr (std::is_signed_v<T> != std::is_signed_v<U>)
        {
            if constexpr (std::is_signed_v<U>)
            {
                assert(u >= 0 &&
                       "lossless_numeric_cast failed due to sign change");
            }
            else
            {
                assert(t >= 0 &&
                       "lossless_numeric_cast failed due to sign change");
            }
        }
    }
    return t;
}

// These comparison functions exist in C++20, but they're useful for
// signedness-agnostic comparisons between types
template <typename A, typename B> constexpr bool cmp_equal(A a, B b) noexcept
{
    if constexpr (std::is_signed_v<A> == std::is_signed_v<B>)
    {
        return a == b;
    }
    else if constexpr (std::is_signed_v<A>)
    {
        return a >= 0 && std::make_unsigned_t<A>(a) == b;
    }
    else
    {
        return b >= 0 && std::make_unsigned_t<B>(b) == a;
    }
}

template <typename A, typename B>
constexpr bool cmp_not_equal(A a, B b) noexcept
{
    return !cmp_equal(a, b);
}

template <typename A, typename B> constexpr bool cmp_less(A a, B b) noexcept
{
    if constexpr (std::is_signed_v<A> == std::is_signed_v<B>)
    {
        return a < b;
    }
    else if constexpr (std::is_signed_v<A>)
    {
        return a < 0 || std::make_unsigned_t<A>(a) < b;
    }
    else
    {
        return b >= 0 && a < std::make_unsigned_t<B>(b);
    }
}

template <typename A, typename B> constexpr bool cmp_greater(A a, B b) noexcept
{
    return cmp_less(b, a);
}

template <typename A, typename B>
constexpr bool cmp_less_equal(A a, B b) noexcept
{
    return !cmp_less(b, a);
}

template <typename A, typename B>
constexpr bool cmp_greater_equal(A a, B b) noexcept
{
    return !cmp_less(a, b);
}

template <typename Dst, typename Src> constexpr Dst clamp_cast(Src v) noexcept
{
    static_assert(std::is_integral_v<Dst> && std::is_integral_v<Src>);

    constexpr auto SrcMin = std::numeric_limits<Src>::min();
    constexpr auto SrcMax = std::numeric_limits<Src>::max();
    constexpr auto DstMin = std::numeric_limits<Dst>::min();
    constexpr auto DstMax = std::numeric_limits<Dst>::max();

    constexpr auto MinNeedsClamp = cmp_less(SrcMin, DstMin);
    constexpr auto MaxNeedsClamp = cmp_greater(SrcMax, DstMax);

    // This could be greatly simplified in C++ 20 using std::cmp_less and
    // cmp_greater.
    if constexpr (MinNeedsClamp && MaxNeedsClamp)
    {
        // Both ends of src are out of range here, so clamp them both.
        return Dst(std::clamp(v, Src(DstMin), Src(DstMax)));
    }
    else if constexpr (MinNeedsClamp)
    {
        // Only need to test against the lower end
        return Dst(std::max(v, Src(DstMin)));
    }
    else if constexpr (MaxNeedsClamp)
    {
        // Only need to test against the upper end.
        return Dst(std::min(v, Src(DstMax)));
    }
    else
    {
        // Using a bracketed cast here instead of parentheses because bracketed
        // numeric conversions do not allow narrowing, so if this could affect
        // the value it would be a compile-time error.
        return Dst{v};
    }
}

// Returns x rounded up to the next multiple of N.
// If x is already a multiple of N, returns x.
template <size_t N, typename T>
RIVE_ALWAYS_INLINE constexpr T round_up_to_multiple_of(T x)
{
    static_assert(N != 0 && (N & (N - 1)) == 0,
                  "math::round_up_to_multiple_of<> only supports powers of 2.");
    return (x + (N - 1)) & ~static_cast<T>(N - 1);
}

// Behaves better with NaN than std::clamp(). (Matching simd::clamp().)
//
//   Returns lo if x == NaN (but std::clamp() returns NaN).
//   Returns hi if hi <= lo.
//   Ignores hi and/or lo if they are NaN.
//
RIVE_ALWAYS_INLINE static float clamp(float x, float lo, float hi)
{
    return fminf(fmaxf(lo, x), hi);
}

// Matches Dart modulus:
// https://api.dart.dev/stable/2.19.0/dart-core/double/operator_modulo.html
RIVE_ALWAYS_INLINE static float positive_mod(float value, float range)
{
    // assert(range > 0.0f);
    if (range < 0)
    {
        range = -range;
    }
    float v = fmodf(value, range);
    if (v < 0.0f)
    {
        v += range;
    }
    return v;
}

inline float degrees_to_radians(float degrees) { return degrees * PI / 180.0f; }

// Returns the smallest number that can be added to 'value', such that
// 'value % alignment' == 0.
template <uint32_t ALIGNMENT> uint32_t padding_to_align_up(uintptr_t value)
{
    constexpr uintptr_t MAX_MULTIPLE_OF_ALIGNMENT =
        std::numeric_limits<uintptr_t>::max() / ALIGNMENT * ALIGNMENT;
    uint32_t padding = (MAX_MULTIPLE_OF_ALIGNMENT - value) % ALIGNMENT;
    assert((value + padding) % ALIGNMENT == 0);
    return padding;
}

template <typename T> uint32_t padding_to_align_up(void* ptr)
{
    return padding_to_align_up<alignof(T)>(reinterpret_cast<uintptr_t>(ptr));
}
} // namespace math

template <typename T> T lerp(const T& a, const T& b, float t)
{
    return a * (1 - t) + b * t;
}
} // namespace rive

#endif
