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
constexpr float EPSILON = 1.f / (1 << 12); // Common threshold for detecting values near zero.

RIVE_MAYBE_UNUSED inline bool nearly_zero(float a, float tolerance = EPSILON)
{
    assert(tolerance >= 0);
    return fabsf(a) <= tolerance;
}

RIVE_MAYBE_UNUSED inline bool nearly_equal(float a, float b, float tolerance = EPSILON)
{
    return nearly_zero(b - a, tolerance);
}

// Performs a floating point division with conformant IEEE 754 behavior for NaN and Inf.
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

// Returns the 1-based index of the most significat bit in x.
//
//   0    -> 0
//   1    -> 1
//   2..3 -> 2
//   4..7 -> 3
//   ...
//
RIVE_ALWAYS_INLINE static uint32_t msb(uint32_t x)
{
    if (x == 0)
    {
        return 0; // __builtin_clz is undefined for x=0, and the double method doesn't work either.
    }
#if defined(__clang__) || defined(__GNUC__)
    return 32 - __builtin_clz(x);
#else
    uint64_t doubleBits = bit_cast<uint64_t>(static_cast<double>(x));
    return (doubleBits >> 52) - 1022;
#endif
}

// Returns x rounded up to the next multiple of N.
// If x is already a multiple of N, returns x.
template <size_t N> RIVE_ALWAYS_INLINE constexpr size_t round_up_to_multiple_of(size_t x)
{
    static_assert(N != 0 && (N & (N - 1)) == 0,
                  "math::round_up_to_multiple_of<> only supports powers of 2.");
    return (x + (N - 1)) & ~(N - 1);
}
} // namespace math

template <typename T> T lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }
} // namespace rive

#endif
