/*
 * Copyright 2022 Rive
 */

// An SSE / NEON / WASM_SIMD library based on clang vector types.
//
// This header makes use of the clang vector builtins specified in https://reviews.llvm.org/D111529.
// This effort in clang is still a work in progress, and compiling this header requires an
// extremely recent version of clang.
//
// To explore the codegen from this header, paste it into https://godbolt.org/, select a recent
// clang compiler, and add an -O3 flag.

#ifndef _RIVE_SIMD_HPP_
#define _RIVE_SIMD_HPP_

#include <cassert>
#include <limits>
#include <stdint.h>

#define SIMD_ALWAYS_INLINE inline __attribute__((always_inline))

// SIMD math can expect conformant IEEE 754 behavior for NaN and Inf.
static_assert(std::numeric_limits<float>::is_iec559,
              "Conformant IEEE 754 behavior for NaN and Inf is required.");

namespace rive
{
namespace simd
{
// The GLSL spec uses "gvec" to denote a vector of unspecified type.
template <typename T, int N>
using gvec = T __attribute__((ext_vector_type(N))) __attribute__((aligned(sizeof(T) * N)));

////// Boolean logic //////
//
// Vector booleans are of type int32_t, where true is ~0 and false is 0. Vector booleans can be
// generated using the builtin boolean operators: ==, !=, <=, >=, <, >
//

// Returns true if all elements in x are equal to 0.
template <int N> SIMD_ALWAYS_INLINE bool any(gvec<int32_t, N> x)
{
    // This particular logic structure gets decent codegen in clang.
    // TODO: __builtin_reduce_or(x) once it's implemented in the compiler.
    for (int i = 0; i < N; ++i)
    {
        if (x[i])
            return true;
    }
    return false;
}

// Returns true if all elements in x are equal to ~0.
template <int N> SIMD_ALWAYS_INLINE bool all(gvec<int32_t, N> x)
{
    // In vector, true is represented by -1 exactly, so we use ~x for "not".
    // TODO: __builtin_reduce_and(x) once it's implemented in the compiler.
    return !any(~x);
}

template <typename T,
          int N,
          typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
SIMD_ALWAYS_INLINE gvec<int32_t, N> isnan(gvec<T, N> x)
{
    return !(x == x);
}

template <typename T, int N, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
constexpr gvec<int32_t, N> isnan(gvec<T, N>)
{
    return {}; // Integer types are never NaN.
}

////// Math //////

// Similar to std::min(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> min(gvec<T, N> a, gvec<T, N> b)
{
#if __has_builtin(__builtin_elementwise_min)
    return __builtin_elementwise_min(a, b);
#else
#pragma message("performance: __builtin_elementwise_min() not supported. Consider updating clang.")
    // Generate the same behavior for NaN as the SIMD builtins. (isnan() is a no-op for int types.)
    return ((b < a) | isnan(a)) ? b : a;
#endif
}

// Similar to std::max(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> max(gvec<T, N> a, gvec<T, N> b)
{
#if __has_builtin(__builtin_elementwise_max)
    return __builtin_elementwise_max(a, b);
#else
#pragma message("performance: __builtin_elementwise_max() not supported. Consider updating clang.")
    // Generate the same behavior for NaN as the SIMD builtins. (isnan() is a no-op for int types.)
    return ((a < b) | isnan(a)) ? b : a;
#endif
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
// If x[i] is an integer type and equal to the minimum representable value, returns x[i].
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> abs(gvec<T, N> x)
{
#if __has_builtin(__builtin_elementwise_abs)
    return __builtin_elementwise_abs(x);
#else
#pragma message("performance: __builtin_elementwise_abs() not supported. Consider updating clang.")
    return x < 0 ? -x : x; // But the negation in the "true" side so we never negate NaN.
#endif
}

////// Loading and storing //////

template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> load(const void* ptr)
{
    gvec<T, N> ret;
    __builtin_memcpy(&ret, ptr, sizeof(T) * N);
    return ret;
}
SIMD_ALWAYS_INLINE gvec<float, 2> load2f(const void* ptr) { return load<float, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<float, 4> load4f(const void* ptr) { return load<float, 4>(ptr); }
SIMD_ALWAYS_INLINE gvec<int32_t, 2> load2i(const void* ptr) { return load<int32_t, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<int32_t, 4> load4i(const void* ptr) { return load<int32_t, 4>(ptr); }
SIMD_ALWAYS_INLINE gvec<uint32_t, 2> load2ui(const void* ptr) { return load<uint32_t, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<uint32_t, 4> load4ui(const void* ptr) { return load<uint32_t, 4>(ptr); }

template <typename T, int N> SIMD_ALWAYS_INLINE void store(void* ptr, gvec<T, N> vec)
{
    __builtin_memcpy(ptr, &vec, sizeof(T) * N);
}

template <typename T, int M, int N>
SIMD_ALWAYS_INLINE gvec<T, M + N> join(gvec<T, M> a, gvec<T, N> b)
{
    T data[M + N];
    __builtin_memcpy(data, &a, sizeof(T) * M);
    __builtin_memcpy(data + M, &b, sizeof(T) * N);
    return load<T, M + N>(data);
}

////// Basic linear algebra //////

template <typename T, int N> SIMD_ALWAYS_INLINE T dot(gvec<T, N> a, gvec<T, N> b)
{
    gvec<T, N> d = a * b;
    if constexpr (N == 2)
    {
        return d.x + d.y;
    }
    else if constexpr (N == 3)
    {
        return d.x + d.y + d.z;
    }
    else if constexpr (N == 4)
    {
        return d.x + d.y + d.z + d.w;
    }
    else
    {
        T s = d[0];
        for (int i = 1; i < N; ++i)
        {
            s += d[i];
        }
        return s;
    }
}

SIMD_ALWAYS_INLINE float cross(gvec<float, 2> a, gvec<float, 2> b)
{
    gvec<float, 2> c = a * b.yx;
    return c.x - c.y;
}

// Linearly interpolates between a and b.
//
// NOTE: mix(a, b, 1) !== b (!!)
//
// The floating point numerics are not precise in the case where t === 1. But overall, this
// structure seems to get better precision for things like chopping cubics on exact cusp points than
// "a*(1 - t) + b*t" (which would return exactly b when t == 1).
template <int N> SIMD_ALWAYS_INLINE gvec<float, N> mix(gvec<float, N> a, gvec<float, N> b, float t)
{
    assert(0 <= t && t < 1);
    return (b - a) * t + a;
}
template <int N>
SIMD_ALWAYS_INLINE gvec<float, N> mix(gvec<float, N> a, gvec<float, N> b, gvec<float, N> t)
{
    assert(simd::all(0 <= t && t < 1));
    return (b - a) * t + a;
}
} // namespace simd
} // namespace rive

#undef SIMD_ALWAYS_INLINE

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
} // namespace rive

#endif
