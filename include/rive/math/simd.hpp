/*
 * Copyright 2022 Rive
 */

// An SSE / NEON / WASM_SIMD library based on clang vector types.
//
// This header makes use of the clang vector builtins specified in
// https://reviews.llvm.org/D111529. This effort in clang is still a work in
// progress, so getting maximum performance from this header requires an
// extremely recent version of clang.
//
// To explore the codegen from this header, paste it into https://godbolt.org/,
// select a recent clang compiler, and add an -O3 flag.

#ifndef _RIVE_SIMD_HPP_
#define _RIVE_SIMD_HPP_

// #define RIVE_SIMD_PERF_WARNINGS

#include <algorithm>
#include <cassert>
#include <limits>
#include <math.h>
#include <stdint.h>
#include <tuple>
#include <type_traits>

#ifdef __SSE__
#include <immintrin.h>
#endif

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#define SIMD_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define __has_builtin(x) 0
#define SIMD_ALWAYS_INLINE inline
#endif

#if __has_builtin(__builtin_memcpy)
#define SIMD_INLINE_MEMCPY __builtin_memcpy
#else
#define SIMD_INLINE_MEMCPY memcpy
#endif

// SIMD math can expect conformant IEEE 754 behavior for NaN and Inf.
static_assert(
    std::numeric_limits<float>::is_iec559,
    "Conformant IEEE 754 behavior for NaN and Inf is required.");

#if defined(__clang__)

namespace rive
{
namespace simd
{
// gvec can be native vectors inside the compiler.
// (The GLSL spec uses "gvec" to denote a vector of unspecified type.)
template <typename T, int N>
using gvec = T __attribute__((ext_vector_type(N)))
__attribute__((aligned(sizeof(T) * N)));

// Vector booleans are masks of integer type, where true is -1 and false is 0.
// Vector booleans masks are generated using the builtin boolean operators: ==,
// !=, <=, >=, <, >
template <typename T> struct extract_element_type;
template <typename T, int N> struct extract_element_type<gvec<T, N>>
{
  using type = T;
};
template <typename T> struct boolean_mask_type
{
  using type = typename extract_element_type<
      decltype(gvec<T, 4>() == gvec<T, 4>())>::type;
};
} // namespace simd
} // namespace rive

#define SIMD_NATIVE_GVEC 1

// NOTE: Due to the build compiling with march=native, on systems with AVX
//  or AVX512 support, it will output those vectors from these functions,
//  but clang will kick out the following warning:
//   AVX vector return of type 'gvec<signed char, 8 + 8 + 8 + 8>' (vector of
//   32 'signed char' values) without 'avx' enabled changes the ABI
//  In release builds, all of the places where these functions are being
//   called should be getting inlined so there's not *technically* an ABI
//   to be breaking, so disabling this should be fine.
#define DISABLE_CLANG_SIMD_ABI_WARNING()                                       \
  _Pragma("clang diagnostic ignored \"-Wpsabi\"")

#define PUSH_DISABLE_CLANG_SIMD_ABI_WARNING()                                  \
  _Pragma("clang diagnostic push") DISABLE_CLANG_SIMD_ABI_WARNING()

#define POP_DISABLE_CLANG_SIMD_ABI_WARNING() _Pragma("clang diagnostic pop")

#else

// gvec needs to be polyfilled with templates.
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: ext_vector_type not supported. Consider using clang.")
#endif
#include "simd_gvec_polyfill.hpp"

#define SIMD_NATIVE_GVEC 0

// With non-native gvec these don't have to do anything
#define DISABLE_CLANG_SIMD_ABI_WARNING()
#define PUSH_DISABLE_CLANG_SIMD_ABI_WARNING()
#define POP_DISABLE_CLANG_SIMD_ABI_WARNING()

#endif

namespace rive
{
namespace simd
{
////// Boolean logic //////
//
// Vector booleans are masks of integer type, where true is -1 and false is 0.
// Vector booleans masks can be generated using the builtin boolean operators:
// ==, !=, <=, >=, <, >
//

// Returns true if all elements in x are equal to 0.
template <typename T, int N> SIMD_ALWAYS_INLINE bool any(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_or) && SIMD_NATIVE_GVEC
  return __builtin_reduce_or(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_or() not supported. Consider updating clang.")
#endif
  // This particular logic structure gets decent codegen in clang.
  for (int i = 0; i < N; ++i)
  {
    if (x[i])
      return true;
  }
  return false;
#endif
}

// Packs a 4-lane boolean mask into the low 4 bits of an int, lane i into bit i.
// Each mask value should be either be 0 or ~0. SSE and NEON extract different
// bits, so partial masks will produce different results.
SIMD_ALWAYS_INLINE uint32_t bitmask(gvec<int32_t, 4> x)
{
#if defined(__SSE__)
  __m128 v;
  SIMD_INLINE_MEMCPY(&v, &x, sizeof(v));
  return static_cast<uint32_t>(_mm_movemask_ps(v));
#elif defined(__aarch64__)
  uint32x4_t v;
  SIMD_INLINE_MEMCPY(&v, &x, sizeof(v));
  const uint32x4_t laneBits = {1, 2, 4, 8};
  return vaddvq_u32(vandq_u32(v, laneBits));
#elif defined(__ARM_NEON)
  // ARMv7 has no across-vector add, so add the halves, then the pair.
  uint32x4_t v;
  SIMD_INLINE_MEMCPY(&v, &x, sizeof(v));
  const uint32x4_t laneBits = {1, 2, 4, 8};
  uint32x4_t bits = vandq_u32(v, laneBits);
  uint32x2_t sum = vadd_u32(vget_low_u32(bits), vget_high_u32(bits));
  return vget_lane_u32(vpadd_u32(sum, sum), 0);
#else
  return static_cast<uint32_t>(
      (x[0] & 1) | (x[1] & 2) | (x[2] & 4) | (x[3] & 8));
#endif
}

// Returns true if all elements in x are equal to ~0.
template <typename T, int N> SIMD_ALWAYS_INLINE bool all(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_and) && SIMD_NATIVE_GVEC
  return __builtin_reduce_and(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  // In vector, true is represented by -1 exactly, so we use ~x for "not".
  return !any(~x);
#endif
}

template <
    typename T,
    int N,
    typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
SIMD_ALWAYS_INLINE gvec<typename boolean_mask_type<T>::type, N> isnan(
    gvec<T, N> x)
{
  return ~(x == x);
}

template <
    typename T,
    int N,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr gvec<typename boolean_mask_type<T>::type, N> isnan(gvec<T, N>)
{
  return {}; // Integer types are never NaN.
}

////// Math //////

// Elementwise ternary expression: "_if ? _then : _else" for each component.
template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> if_then_else(
    gvec<typename boolean_mask_type<T>::type, N> _if,
    gvec<T, N> _then,
    gvec<T, N> _else)
{
#if defined(__clang_major__) && __clang_major__ >= 13
  // The '?:' operator supports a vector condition beginning in clang 13.
  return _if ? _then : _else;
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: vectorized '?:' operator not supported. Consider updating clang.")
#endif
  gvec<T, N> ret{};
  for (int i = 0; i < N; ++i)
    ret[i] = _if[i] ? _then[i] : _else[i];
  return ret;
#endif
}

// Similar to std::min(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> min(gvec<T, N> a, gvec<T, N> b)
{
#if SIMD_NATIVE_GVEC && (defined(__arm__) || defined(__wasm__))
#if __has_builtin(__builtin_elementwise_min)
#if !defined(__ARM_FEATURE_NUMERIC_MAXMIN)
  // 32-bit ARM before ARMv8 and wasm have no vector instruction with the
  // builtin's NaN semantics, so for floats it becomes a call to fminf() per
  // element.
  if constexpr (std::is_floating_point_v<T>)
  {
    return if_then_else(b < a || isnan(a), b, a);
  }
  else
#endif
  {
    return __builtin_elementwise_min(a, b);
  }
#else
  return if_then_else(b < a || isnan(a), b, a);
#endif
#elif SIMD_NATIVE_GVEC && __has_builtin(__builtin_elementwise_min)
  return __builtin_elementwise_min(a, b);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_elementwise_min() not supported. Consider updating clang.")
#endif
  // Generate the same behavior for NaN as the SIMD builtins. (isnan() is a
  // no-op for int types.)
  return if_then_else(b < a || isnan(a), b, a);
#endif
}

// Similar to std::max(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> max(gvec<T, N> a, gvec<T, N> b)
{
#if SIMD_NATIVE_GVEC && (defined(__arm__) || defined(__wasm__))
#if __has_builtin(__builtin_elementwise_max)
#if !defined(__ARM_FEATURE_NUMERIC_MAXMIN)
  // See min().
  if constexpr (std::is_floating_point_v<T>)
  {
    return if_then_else(a < b || isnan(a), b, a);
  }
  else
#endif
  {
    return __builtin_elementwise_max(a, b);
  }
#else
  return if_then_else(a < b || isnan(a), b, a);
#endif
#elif SIMD_NATIVE_GVEC && __has_builtin(__builtin_elementwise_max)
  return __builtin_elementwise_max(a, b);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_elementwise_max() not supported. Consider updating clang.")
#endif
  // Generate the same behavior for NaN as the SIMD builtins. (isnan() is a
  // no-op for int types.)
  return if_then_else(a < b || isnan(a), b, a);
#endif
}

// Faster than simd::min/max but does not have its NaN-handling behavior
template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> minFast(gvec<T, N> a, gvec<T, N> b)
{
  return if_then_else(b < a, b, a);
}

template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> maxFast(gvec<T, N> a, gvec<T, N> b)
{
  return if_then_else(a < b, b, a);
}

// Unlike std::clamp(), simd::clamp() always returns a value between lo and hi.
//
//   Returns lo if x == NaN, but std::clamp() returns NaN.
//   Returns hi if hi <= lo.
//   Ignores hi and/or lo if they are NaN.
//
template <typename T, int N>
SIMD_ALWAYS_INLINE gvec<T, N> clamp(gvec<T, N> x, gvec<T, N> lo, gvec<T, N> hi)
{
  return min(max(lo, x), hi);
}

// Returns the absolute value of x per element, with one exception:
// If x[i] is an integer type and equal to the minimum representable value,
// returns x[i].
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> abs(gvec<T, N> x)
{
#if __has_builtin(__builtin_elementwise_abs) && SIMD_NATIVE_GVEC
  return __builtin_elementwise_abs(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_elementwise_abs() not supported. Consider updating clang.")
#endif
  return if_then_else(
      x < (T)0,
      -x,
      x); // Negate on the "true" side so we never negate NaN.
#endif
}

template <typename T, int N>
SIMD_ALWAYS_INLINE typename std::enable_if<std::is_integral<T>::value, T>::type
reduce_add(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_add) && SIMD_NATIVE_GVEC
  return __builtin_reduce_add(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T s = x[0];
  for (int i = 1; i < N; ++i)
    s += x[i];
  return s;
#endif
}

template <typename T, int N>
SIMD_ALWAYS_INLINE typename std::enable_if<!std::is_integral<T>::value, T>::type
reduce_add(gvec<T, N> x)
{
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T s = x[0];
  for (int i = 1; i < N; ++i)
    s += x[i];
  return s;
}

template <typename T, int N> SIMD_ALWAYS_INLINE T reduce_min(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_and) && SIMD_NATIVE_GVEC
  return __builtin_reduce_min(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T reduced = x[0];
  for (int i = 1; i < N; ++i)
    reduced = std::min(reduced, x[i]);
  return reduced;
#endif
}

template <typename T, int N> SIMD_ALWAYS_INLINE T reduce_max(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_and) && SIMD_NATIVE_GVEC
  return __builtin_reduce_max(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T reduced = x[0];
  for (int i = 1; i < N; ++i)
    reduced = std::max(reduced, x[i]);
  return reduced;
#endif
}

template <typename T, int N> SIMD_ALWAYS_INLINE T reduce_and(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_and) && SIMD_NATIVE_GVEC
  return __builtin_reduce_and(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T reduced = x[0];
  for (int i = 1; i < N; ++i)
    reduced &= x[i];
  return reduced;
#endif
}

template <typename T, int N> SIMD_ALWAYS_INLINE T reduce_or(gvec<T, N> x)
{
#if __has_builtin(__builtin_reduce_and) && SIMD_NATIVE_GVEC
  return __builtin_reduce_or(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_reduce_and() not supported. Consider updating clang.")
#endif
  T reduced = x[0];
  for (int i = 1; i < N; ++i)
    reduced |= x[i];
  return reduced;
#endif
}

////// Type conversion //////

template <typename U, typename T, int N>
SIMD_ALWAYS_INLINE gvec<U, N> cast(gvec<T, N> x)
{
#if __has_builtin(__builtin_convertvector) && SIMD_NATIVE_GVEC
  return __builtin_convertvector(x, gvec<U, N>);
#else
  gvec<U, N> y{};
  for (int i = 0; i < N; ++i)
    y[i] = static_cast<U>(x[i]);
  return y;
#endif
}

////// Floating Point Functions //////

template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> copysign(gvec<float, N> x, gvec<float, N> y)
{
  constexpr static uint32_t SIGN_BIT = 0x80000000u;
  // Type punning is free in vector registers.
  gvec<uint32_t, N> bitsX, bitsY;
  SIMD_INLINE_MEMCPY(&bitsX, &x, sizeof(x));
  SIMD_INLINE_MEMCPY(&bitsY, &y, sizeof(y));
  gvec<uint32_t, N> bitsRet = (bitsY & SIGN_BIT) | (bitsX & ~SIGN_BIT);
  gvec<float, N> ret;
  SIMD_INLINE_MEMCPY(&ret, &bitsRet, sizeof(ret));
  return ret;
}

// On pre-ARMv8 NEON, floor/ceil go through scalar operations, so use this
// implementation which uses integer truncation (inf/NaN pass through
// unchanged).
template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> floorByIntConversion(gvec<float, N> x)
{
  auto isSmall = abs(x) < 8388608.f; // 2^23, floats this large are all ints
  gvec<float, N> t = cast<float>(
      cast<int32_t>(if_then_else(isSmall, x, gvec<float, N>(0))));
  t = if_then_else(t > x, t - 1.f, t);
  // The result has x's sign, which keeps -0.
  return if_then_else(isSmall, copysign(t, x), x);
}

template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> ceilByIntConversion(gvec<float, N> x)
{
  auto isSmall = abs(x) < 8388608.f; // 2^23, floats this large are all ints
  gvec<float, N> t = cast<float>(
      cast<int32_t>(if_then_else(isSmall, x, gvec<float, N>(0))));
  t = if_then_else(t < x, t + 1.f, t);
  // The result has x's sign, which keeps -0 (e.g., ceil(-0.5)).
  return if_then_else(isSmall, copysign(t, x), x);
}

template <int N> SIMD_ALWAYS_INLINE gvec<float, N> floor(gvec<float, N> x)
{
#if SIMD_NATIVE_GVEC && defined(__arm__) &&                                    \
    !defined(__ARM_FEATURE_DIRECTED_ROUNDING)
  // 32-bit ARM before ARMv8 can't round floats in vector registers, so the
  // builtin becomes a call to floorf() per element.
  return floorByIntConversion(x);
#elif SIMD_NATIVE_GVEC && __has_builtin(__builtin_elementwise_floor)
  return __builtin_elementwise_floor(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_elementwise_floor() not supported. Consider updating clang.")
#endif
  for (int i = 0; i < N; ++i)
    x[i] = floorf(x[i]);
  return x;
#endif
}

template <int N> SIMD_ALWAYS_INLINE gvec<float, N> ceil(gvec<float, N> x)
{
#if SIMD_NATIVE_GVEC && defined(__arm__) &&                                    \
    !defined(__ARM_FEATURE_DIRECTED_ROUNDING)
  // 32-bit ARM before ARMv8 can't round floats in vector registers, so the
  // builtin becomes a call to ceilf() per element.
  return ceilByIntConversion(x);
#elif SIMD_NATIVE_GVEC && __has_builtin(__builtin_elementwise_ceil)
  return __builtin_elementwise_ceil(x);
#else
#ifdef RIVE_SIMD_PERF_WARNINGS
#pragma message(                                                               \
    "performance: __builtin_elementwise_ceil() not supported. Consider updating clang.")
#endif
  for (int i = 0; i < N; ++i)
    x[i] = ceilf(x[i]);
  return x;
#endif
}

// IEEE compliant sqrt.
template <int N> SIMD_ALWAYS_INLINE gvec<float, N> sqrt(gvec<float, N> x)
{
  // There isn't an elementwise builtin for sqrt. We define
  // architecture-specific specializations of this function later.
  for (int i = 0; i < N; ++i)
    x[i] = sqrtf(x[i]);
  return x;
}

#ifdef __SSE__
template <> SIMD_ALWAYS_INLINE gvec<float, 4> sqrt(gvec<float, 4> x)
{
  __m128 _x;
  SIMD_INLINE_MEMCPY(&_x, &x, sizeof(float) * 4);
  _x = _mm_sqrt_ps(_x);
  SIMD_INLINE_MEMCPY(&x, &_x, sizeof(float) * 4);
  return x;
}

template <> SIMD_ALWAYS_INLINE gvec<float, 2> sqrt(gvec<float, 2> x)
{
  __m128 _x;
  SIMD_INLINE_MEMCPY(&_x, &x, sizeof(float) * 2);
  _x = _mm_sqrt_ps(_x);
  SIMD_INLINE_MEMCPY(&x, &_x, sizeof(float) * 2);
  return x;
}
#endif

#ifdef __aarch64__
template <> SIMD_ALWAYS_INLINE gvec<float, 4> sqrt(gvec<float, 4> x)
{
  float32x4_t _x;
  SIMD_INLINE_MEMCPY(&_x, &x, sizeof(float) * 4);
  _x = vsqrtq_f32(_x);
  SIMD_INLINE_MEMCPY(&x, &_x, sizeof(float) * 4);
  return x;
}

template <> SIMD_ALWAYS_INLINE gvec<float, 2> sqrt(gvec<float, 2> x)
{
  float32x2_t _x;
  SIMD_INLINE_MEMCPY(&_x, &x, sizeof(float) * 2);
  _x = vsqrt_f32(_x);
  SIMD_INLINE_MEMCPY(&x, &_x, sizeof(float) * 2);
  return x;
}
#endif

// This will only be present when building with Emscripten and "-msimd128".
#if __has_builtin(__builtin_wasm_sqrt_f32x4) && SIMD_NATIVE_GVEC
template <> SIMD_ALWAYS_INLINE gvec<float, 4> sqrt(gvec<float, 4> x)
{
  return __builtin_wasm_sqrt_f32x4(x);
}

template <> SIMD_ALWAYS_INLINE gvec<float, 2> sqrt(gvec<float, 2> x)
{
  gvec<float, 4> _x{x.x, x.y};
  _x = __builtin_wasm_sqrt_f32x4(_x);
  return _x.xy;
}
#endif

#if defined(__SSE__) || defined(__ARM_NEON)
// Only defined where the hardware has a reciprocal square root estimate.
#define RIVE_HAS_FAST_INVERSE_SQRT

// Approximates 1 / sqrt(x) from the hardware estimate.
SIMD_ALWAYS_INLINE gvec<float, 4> inverseSqrtFast(gvec<float, 4> x)
{
#if defined(__SSE__)
  // This result has 12 bits of precision
  __m128 xv;
  SIMD_INLINE_MEMCPY(&xv, &x, sizeof(xv));
  __m128 yv = _mm_rsqrt_ps(xv);
#else
  // vrsqrteq_f32 has only 8 bits of precision, so use vrsqrtsq_f32 to perform
  // a Newton-Raphson iteration.
  float32x4_t xv;
  SIMD_INLINE_MEMCPY(&xv, &x, sizeof(xv));
  float32x4_t yv = vrsqrteq_f32(xv);
  yv = vmulq_f32(yv, vrsqrtsq_f32(xv, vmulq_f32(yv, yv)));
#endif
  gvec<float, 4> y;
  SIMD_INLINE_MEMCPY(&y, &yv, sizeof(y));
  return y;
}
#endif

// Returns "(x + 127) / 255", for x in the range 0..255*255.
template <int N>
SIMD_ALWAYS_INLINE gvec<uint16_t, N> div255(gvec<uint16_t, N> x)
{
  assert(all(x <= 255u * 255u));
  x += 128;
  return (x + (x >> 8)) >> 8;
}

// Approximates acos(x) within 0.96 degrees, using the rational polynomial:
//
//     acos(x) ~= (bx^3 + ax) / (dx^4 + cx^2 + 1) + pi/2
//
// See: https://stackoverflow.com/a/36387954
#define SIMD_FAST_ACOS_MAX_ERROR 0.0167552f // .96 degrees
template <int N> SIMD_ALWAYS_INLINE gvec<float, N> fast_acos(gvec<float, N> x)
{
  constexpr static float a = -0.939115566365855f;
  constexpr static float b = 0.9217841528914573f;
  constexpr static float c = -1.2845906244690837f;
  constexpr static float d = 0.295624144969963174f;
  constexpr static float pi_over_2 = 1.5707963267948966f;
  auto xx = x * x;
  auto numer = b * xx + a;
  auto denom = xx * (d * xx + c) + 1.f;
  return x * (numer / denom) + pi_over_2;
}

////// Loading and storing //////

template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> load(const void* ptr)
{
  gvec<T, N> ret;
  SIMD_INLINE_MEMCPY(&ret, ptr, sizeof(T) * N);
  return ret;
}
SIMD_ALWAYS_INLINE gvec<float, 2> load2f(const void* ptr)
{
  return load<float, 2>(ptr);
}
SIMD_ALWAYS_INLINE gvec<float, 4> load4f(const void* ptr)
{
  return load<float, 4>(ptr);
}
SIMD_ALWAYS_INLINE gvec<int32_t, 2> load2i(const void* ptr)
{
  return load<int32_t, 2>(ptr);
}
SIMD_ALWAYS_INLINE gvec<int32_t, 4> load4i(const void* ptr)
{
  return load<int32_t, 4>(ptr);
}
SIMD_ALWAYS_INLINE gvec<uint32_t, 2> load2ui(const void* ptr)
{
  return load<uint32_t, 2>(ptr);
}
SIMD_ALWAYS_INLINE gvec<uint32_t, 4> load4ui(const void* ptr)
{
  return load<uint32_t, 4>(ptr);
}

template <typename T, int N>
SIMD_ALWAYS_INLINE void store(void* dst, gvec<T, N> vec)
{
  SIMD_INLINE_MEMCPY(dst, &vec, sizeof(T) * N);
}

////// Column-major (transposed) loads //////

// This structure is used instead of tuples because MSVC enabled 8-byte packing
// for tuples which breaks float4 since it requires 16-byte alignment for
// aligned loads. See https://github.com/microsoft/STL/blob/main/stl/inc/tuple
template <typename T> struct gvec4x4
{
  gvec<T, 4> cols[4];

  // For structured bindings, e.g., "auto [x, y, z, w] = load4x4f(m)".
  template <size_t I> gvec<T, 4>& get() { return cols[I]; }
  template <size_t I> const gvec<T, 4>& get() const { return cols[I]; }
};

#if defined(__ARM_NEON)
SIMD_ALWAYS_INLINE gvec4x4<float> load4x4f(const float* matrix)
{
  float32x4x4_t m = vld4q_f32(matrix);
  gvec4x4<float> result;
  SIMD_INLINE_MEMCPY(&result.cols[0], &m.val[0], sizeof(result.cols[0]));
  SIMD_INLINE_MEMCPY(&result.cols[1], &m.val[1], sizeof(result.cols[1]));
  SIMD_INLINE_MEMCPY(&result.cols[2], &m.val[2], sizeof(result.cols[2]));
  SIMD_INLINE_MEMCPY(&result.cols[3], &m.val[3], sizeof(result.cols[3]));
  return result;
}
#elif defined(__SSE__)
SIMD_ALWAYS_INLINE gvec4x4<float> load4x4f(const float* m)
{
  __m128 r0, r1, r2, r3;
  SIMD_INLINE_MEMCPY(&r0, m + 4 * 0, sizeof(r0));
  SIMD_INLINE_MEMCPY(&r1, m + 4 * 1, sizeof(r1));
  SIMD_INLINE_MEMCPY(&r2, m + 4 * 2, sizeof(r2));
  SIMD_INLINE_MEMCPY(&r3, m + 4 * 3, sizeof(r3));
  _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
  gvec4x4<float> result;
  SIMD_INLINE_MEMCPY(&result.cols[0], &r0, sizeof(result.cols[0]));
  SIMD_INLINE_MEMCPY(&result.cols[1], &r1, sizeof(result.cols[1]));
  SIMD_INLINE_MEMCPY(&result.cols[2], &r2, sizeof(result.cols[2]));
  SIMD_INLINE_MEMCPY(&result.cols[3], &r3, sizeof(result.cols[3]));
  return result;
}
#else
SIMD_ALWAYS_INLINE gvec4x4<float> load4x4f(const float* m)
{
  return {{
      {m[0], m[4], m[8], m[12]},
      {m[1], m[5], m[9], m[13]},
      {m[2], m[6], m[10], m[14]},
      {m[3], m[7], m[11], m[15]},
  }};
}
#endif

SIMD_ALWAYS_INLINE gvec4x4<float> transpose4x4f(
    gvec<float, 4> row0,
    gvec<float, 4> row1,
    gvec<float, 4> row2,
    gvec<float, 4> row3)
{
#if defined(__SSE__)
  __m128 r0, r1, r2, r3;
  SIMD_INLINE_MEMCPY(&r0, &row0, sizeof(r0));
  SIMD_INLINE_MEMCPY(&r1, &row1, sizeof(r1));
  SIMD_INLINE_MEMCPY(&r2, &row2, sizeof(r2));
  SIMD_INLINE_MEMCPY(&r3, &row3, sizeof(r3));
  _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
  gvec4x4<float> result;
  SIMD_INLINE_MEMCPY(&result.cols[0], &r0, sizeof(result.cols[0]));
  SIMD_INLINE_MEMCPY(&result.cols[1], &r1, sizeof(result.cols[1]));
  SIMD_INLINE_MEMCPY(&result.cols[2], &r2, sizeof(result.cols[2]));
  SIMD_INLINE_MEMCPY(&result.cols[3], &r3, sizeof(result.cols[3]));
  return result;
#elif defined(__aarch64__)
  float32x4_t r0, r1, r2, r3;
  SIMD_INLINE_MEMCPY(&r0, &row0, sizeof(r0));
  SIMD_INLINE_MEMCPY(&r1, &row1, sizeof(r1));
  SIMD_INLINE_MEMCPY(&r2, &row2, sizeof(r2));
  SIMD_INLINE_MEMCPY(&r3, &row3, sizeof(r3));
  // Interleave pairs of rows, then pairs of 64-bit halves.
  float32x4_t t0 = vtrn1q_f32(r0, r1);
  float32x4_t t1 = vtrn2q_f32(r0, r1);
  float32x4_t t2 = vtrn1q_f32(r2, r3);
  float32x4_t t3 = vtrn2q_f32(r2, r3);
  float32x4_t col0 = vreinterpretq_f32_f64(
      vtrn1q_f64(vreinterpretq_f64_f32(t0), vreinterpretq_f64_f32(t2)));
  float32x4_t col1 = vreinterpretq_f32_f64(
      vtrn1q_f64(vreinterpretq_f64_f32(t1), vreinterpretq_f64_f32(t3)));
  float32x4_t col2 = vreinterpretq_f32_f64(
      vtrn2q_f64(vreinterpretq_f64_f32(t0), vreinterpretq_f64_f32(t2)));
  float32x4_t col3 = vreinterpretq_f32_f64(
      vtrn2q_f64(vreinterpretq_f64_f32(t1), vreinterpretq_f64_f32(t3)));
  gvec4x4<float> result;
  SIMD_INLINE_MEMCPY(&result.cols[0], &col0, sizeof(result.cols[0]));
  SIMD_INLINE_MEMCPY(&result.cols[1], &col1, sizeof(result.cols[1]));
  SIMD_INLINE_MEMCPY(&result.cols[2], &col2, sizeof(result.cols[2]));
  SIMD_INLINE_MEMCPY(&result.cols[3], &col3, sizeof(result.cols[3]));
  return result;
#else
  return {{
      {row0[0], row1[0], row2[0], row3[0]},
      {row0[1], row1[1], row2[1], row3[1]},
      {row0[2], row1[2], row2[2], row3[2]},
      {row0[3], row1[3], row2[3], row3[3]},
  }};
#endif
}

PUSH_DISABLE_CLANG_SIMD_ABI_WARNING()

template <typename T, int M, int N>
SIMD_ALWAYS_INLINE gvec<T, M + N> join(gvec<T, M> a, gvec<T, N> b)
{
  T data[M + N];
  SIMD_INLINE_MEMCPY(data, &a, sizeof(T) * M);
  SIMD_INLINE_MEMCPY(data + M, &b, sizeof(T) * N);
  return load<T, M + N>(data);
}

template <typename T, int M, int N, int O>
SIMD_ALWAYS_INLINE gvec<T, M + N + O> join(
    gvec<T, M> a,
    gvec<T, N> b,
    gvec<T, O> c)
{
  T data[M + N + O];
  SIMD_INLINE_MEMCPY(data, &a, sizeof(T) * M);
  SIMD_INLINE_MEMCPY(data + M, &b, sizeof(T) * N);
  SIMD_INLINE_MEMCPY(data + M + N, &c, sizeof(T) * O);
  return load<T, M + N + O>(data);
}

template <typename T, int M, int N, int O, int P>
SIMD_ALWAYS_INLINE gvec<T, M + N + O + P> join(
    gvec<T, M> a,
    gvec<T, N> b,
    gvec<T, O> c,
    gvec<T, P> d)
{
  T data[M + N + O + P];
  SIMD_INLINE_MEMCPY(data, &a, sizeof(T) * M);
  SIMD_INLINE_MEMCPY(data + M, &b, sizeof(T) * N);
  SIMD_INLINE_MEMCPY(data + M + N, &c, sizeof(T) * O);
  SIMD_INLINE_MEMCPY(data + M + N + O, &d, sizeof(T) * P);
  return load<T, M + N + O + P>(data);
}

template <typename T>
SIMD_ALWAYS_INLINE gvec<T, 4> zip(gvec<T, 2> a, gvec<T, 2> b)
{
#if __has_builtin(__builtin_shufflevector) && SIMD_NATIVE_GVEC
  return __builtin_shufflevector(a, b, 0, 2, 1, 3);
#else
  return gvec<T, 4>{a.x, b.x, a.y, b.y};
#endif
}

template <typename T>
SIMD_ALWAYS_INLINE gvec<T, 8> zip(gvec<T, 4> a, gvec<T, 4> b)
{
#if __has_builtin(__builtin_shufflevector) && SIMD_NATIVE_GVEC
  return __builtin_shufflevector(a, b, 0, 4, 1, 5, 2, 6, 3, 7);
#else
  return gvec<T, 8>{a.x, b.x, a.y, b.y, a.z, b.z, a.w, b.w};
#endif
}

template <typename T>
SIMD_ALWAYS_INLINE gvec<T, 16> zip(gvec<T, 8> a, gvec<T, 8> b)
{
#if __has_builtin(__builtin_shufflevector) && SIMD_NATIVE_GVEC
  return __builtin_shufflevector(
      a,
      b,
      0,
      8,
      1,
      9,
      2,
      10,
      3,
      11,
      4,
      12,
      5,
      13,
      6,
      14,
      7,
      15);
#else
  return gvec<T, 16>{
      a[0],
      b[0],
      a[1],
      b[1],
      a[2],
      b[2],
      a[3],
      b[3],
      a[4],
      b[4],
      a[5],
      b[5],
      a[6],
      b[6],
      a[7],
      b[7]};
#endif
}

template <typename T, int N>
SIMD_ALWAYS_INLINE
    typename std::enable_if<N != 2 && N != 4 && N != 8, gvec<T, N * 2>>::type
    zip(gvec<T, N> a, gvec<T, N> b)
{
  gvec<T, N * 2> ret{};
  for (int i = 0; i < N; ++i)
  {
    ret[i * 2] = a[i];
    ret[i * 2 + 1] = b[i];
  }
  return ret;
}

POP_DISABLE_CLANG_SIMD_ABI_WARNING()

////// Basic linear algebra //////

template <typename T, int N>
SIMD_ALWAYS_INLINE T dot(gvec<T, N> a, gvec<T, N> b)
{
  return reduce_add(a * b);
}

SIMD_ALWAYS_INLINE float cross(gvec<float, 2> a, gvec<float, 2> b)
{
  auto c = a * b.yx;
  return c.x - c.y;
}

// Linearly interpolates between a and b.
//
// NOTE: mix(a, b, 1) !== b (!!)
//
// The floating point numerics are not precise in the case where t === 1. But
// overall, this structure seems to get better precision for things like
// chopping cubics on exact cusp points than "a*(1 - t) + b*t" (which would
// return exactly b when t == 1).
template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> mix(
    gvec<float, N> a,
    gvec<float, N> b,
    gvec<float, N> t)
{
  assert(simd::all(0.f <= t && t < 1.f));
  return (b - a) * t + a;
}
// Called when it doesn't matter if mix(a, b, 1) is only ~= b (it may not be
// precisely b).
template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> unchecked_mix(
    gvec<float, N> a,
    gvec<float, N> b,
    gvec<float, N> t)
{
  return (b - a) * t + a;
}
// Returns precisely 'a' if t==0 and precisely 'b' if t==1.
template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> precise_mix(
    gvec<float, N> a,
    gvec<float, N> b,
    gvec<float, N> t)
{
  return a * (1.f - t) + b * t;
}
} // namespace simd
} // namespace rive

namespace rive
{
template <int N> using vec = simd::gvec<float, N>;
using float2 = vec<2>;
using float4 = vec<4>;

template <int N> using ivec = simd::gvec<int32_t, N>;
using int2 = ivec<2>;
using int4 = ivec<4>;

template <int N> using uvec = simd::gvec<uint32_t, N>;
using uint2 = uvec<2>;
using uint4 = uvec<4>;

using int8x8 = simd::gvec<int8_t, 8>;
using int8x16 = simd::gvec<int8_t, 16>;
using int8x32 = simd::gvec<int8_t, 32>;

using uint8x8 = simd::gvec<uint8_t, 8>;
using uint8x16 = simd::gvec<uint8_t, 16>;
using uint8x32 = simd::gvec<uint8_t, 32>;

using int16x4 = simd::gvec<int16_t, 4>;
using int16x8 = simd::gvec<int16_t, 8>;
using int16x16 = simd::gvec<int16_t, 16>;

using uint16x4 = simd::gvec<uint16_t, 4>;
using uint16x8 = simd::gvec<uint16_t, 8>;
using uint16x16 = simd::gvec<uint16_t, 16>;

using int64x2 = simd::gvec<int64_t, 2>;
using int64x4 = simd::gvec<int64_t, 4>;

using uint64x2 = simd::gvec<uint64_t, 2>;
using uint64x4 = simd::gvec<uint64_t, 4>;

} // namespace rive

// The tuple protocol for gvec4x4, for structured bindings.
namespace std
{
template <typename T>
struct tuple_size<rive::simd::gvec4x4<T>> : integral_constant<size_t, 4>
{};

template <size_t I, typename T> struct tuple_element<I, rive::simd::gvec4x4<T>>
{
  using type = rive::simd::gvec<T, 4>;
};
} // namespace std

#undef SIMD_INLINE_MEMCPY
#undef SIMD_ALWAYS_INLINE

#endif
