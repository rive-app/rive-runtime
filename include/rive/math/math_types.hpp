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

// Lossless cast function that asserts on overflow
template <typename T, typename U> T lossless_numeric_cast(U u)
{
    T t = static_cast<T>(u);
    assert(static_cast<U>(t) == u);
    return t;
}

// Attempt to generate a "clz" assembly instruction.
RIVE_ALWAYS_INLINE static int clz32(uint32_t x)
{
    assert(x != 0);
#if __has_builtin(__builtin_clz)
    return __builtin_clz(x);
#else
    uint64_t doubleBits = bit_cast<uint64_t>(static_cast<double>(x));
    return 1054 - (doubleBits >> 52);
#endif
}

RIVE_ALWAYS_INLINE static int clz64(uint64_t x)
{
    assert(x != 0);
#if __has_builtin(__builtin_clzll)
    return __builtin_clzll(x);
#else
    uint32_t hi32 = x >> 32;
    return hi32 != 0 ? clz32(hi32) : 32 + clz32(x & 0xffffffff);
#endif
}

// Returns the 1-based index of the most significat bit in x.
//
//   0    -> 0
//   1    -> 1
//   2..3 -> 2
//   4..7 -> 3
//   ...
//
RIVE_ALWAYS_INLINE static uint32_t msb(uint32_t x) { return x != 0 ? 32 - clz32(x) : 0; }

// Attempt to generate a "rotl" (rotate-left) assembly instruction.
RIVE_ALWAYS_INLINE static uint32_t rotateleft32(uint32_t x, int y)
{
#if __has_builtin(__builtin_rotateleft32)
    return __builtin_rotateleft32(x, y);
#else
    return (x << y) | (x >> (32 - y));
#endif
}

// Returns x rounded up to the next multiple of N.
// If x is already a multiple of N, returns x.
template <size_t N, typename T> RIVE_ALWAYS_INLINE constexpr T round_up_to_multiple_of(T x)
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
} // namespace math

template <typename T> T lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }
} // namespace rive

#endif
