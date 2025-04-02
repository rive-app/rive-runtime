/*
 * Copyright 2025 Rive
 */

// Provides GLSL-specific #defines and declarations that enable our shaders to
// be compiled as C++ and unit tested.

#ifdef _MSC_VER
#error glsl cross-compiling requires the clang/gcc vector extension
#else

#include "rive/math/simd.hpp"
#include "rive/math/math_types.hpp"
#include <array>

using namespace rive;
#define INLINE inline
#define OUT(T) T&
#define make_float4(x, y, z, w)                                                \
    float4 { x, y, z, w }
#define make_float2(x, y)                                                      \
    float2 { x, y }

using math::PI;
using simd::abs;
using simd::any;
using simd::clamp;
using simd::dot;
using simd::max;
using simd::min;
using float3 = vec<3>;
using float2x2 = std::array<float2, 2>;
using half4 = float4;
using half3 = float3;
using half2 = float2;
using half = float;
using uint = uint32_t;
using ushort = uint16_t;

inline half4 make_half4(half x, half y, half z, half w) { return {x, y, z, w}; }
inline half4 make_half4(half3 xyz, half w) { return {xyz.x, xyz.y, xyz.z, w}; }
inline half4 make_half4(half x) { return {x, x, x, x}; }
inline half3 make_half3(half x, half y, half z) { return {x, y, z}; }
inline half3 make_half3(half x) { return {x, x, x}; }
inline half2 make_half2(half x, half y) { return {x, y}; }
inline half2 make_half2(half x) { return {x, x}; }

using half2x3 = std::array<half3, 2>;
inline half2x3 make_half2x3(half3 c0, half3 c1) { return {c0, c1}; }
inline half3 MUL(half2x3 m, half2 v)
{
    half3 ret;
    for (int i = 0; i < 3; ++i)
    {
        ret[i] = m[0][i] * v[0] + m[1][i] * v[1];
    }
    return ret;
}

using half3x3 = std::array<half3, 3>;
inline half3x3 make_half3x3(half3 c0, half3 c1, half3 c2)
{
    return {c0, c1, c2};
}
inline half3 MUL(half3x3 m, half3 v)
{
    half3 ret;
    for (int i = 0; i < 3; ++i)
    {
        ret[i] = m[0][i] * v[0] + m[1][i] * v[1] + m[2][i] * v[2];
    }
    return ret;
}

inline float clamp(float x, float lo, float hi)
{
    return std::clamp(x, lo, hi);
}
inline float min(float x, float y) { return std::min(x, y); }
inline float max(float x, float y) { return std::max(x, y); }

template <typename T> T inversesqrt(T x) { return 1 / sqrt(x); }
template <int N> float length(vec<N> x) { return std::sqrt(dot(x, x)); }
template <int N> vec<N> normalize(vec<N> x) { return x / length(x); }
template <int N> vec<N> sign(vec<N> x)
{
    return simd::if_then_else(x < 0,
                              vec<N>(-1),
                              simd::if_then_else(x > 0, vec<N>(1), vec<N>(0)));
}
template <int N> vec<N> mix(vec<N> a, vec<N> b, ivec<N> t)
{
    return simd::if_then_else(t, b, a);
}

template <typename T, int N>
ivec<N> equal(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x == y;
}

template <typename T, int N>
ivec<N> notEqual(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x != y;
}

template <typename T, int N>
ivec<N> lessThanEqual(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x <= y;
}

template <typename T, int N>
ivec<N> greaterThanEqual(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x >= y;
}

#endif
