/*
 * Copyright 2025 Rive
 */

// Provides GLSL-specific #defines and declarations that enable our shaders to
// be compiled as C++ and unit tested.

#include "rive/math/simd.hpp"
#include "rive/math/math_types.hpp"
#include <array>
#include <cmath>
#include <cstring>

using namespace rive;
#define INLINE inline
#define OUT(T) T&
#ifndef RIVE_GLSL_CROSS_WANT_CONSTRUCTORS
#define make_float4(x, y, z, w)                                                \
    float4 { x, y, z, w }
#define make_float2(x, y)                                                      \
    float2 { x, y }
#endif

using math::PI;
using simd::abs;
using simd::any;
using simd::clamp;
using simd::dot;
using simd::max;
using simd::min;
using simd::mix;
using simd::sqrt;
using std::sqrt;
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
inline half make_half(float x) { return x; }

// Vector booleans are masks of integer type; see simd.hpp.
using bool2 = decltype(float2() == float2());

// GLSL builtins with no C++ equivalent.
inline float fract(float x) { return x - std::floor(x); }
inline float sign(float x) { return x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f); }
inline float uintBitsToFloat(uint u)
{
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}
inline uint floatBitsToUint(float f)
{
    uint u;
    memcpy(&u, &f, sizeof(u));
    return u;
}
// Declared, not defined: shaders that merely mention it still compile, and one
// that actually calls it gets a link error instead of a wrong answer.
half2 unpackHalf2x16(uint);

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

using half4x4 = std::array<half4, 4>;
inline half4x4 make_half4x4(half4 c0, half4 c1, half4 c2, half4 c3)
{
    return {c0, c1, c2, c3};
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
ivec<N> lessThan(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x < y;
}

template <typename T, int N>
ivec<N> greaterThanEqual(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x >= y;
}

template <typename T, int N>
ivec<N> greaterThan(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x > y;
}

// Scaffolding for shaders that use GLSL constructor syntax -- float2(x, x) --
// or read the flush uniforms. common.glsl needs both. Opt in by defining
// RIVE_GLSL_CROSS_WANT_CONSTRUCTORS before including this file; it is off by
// default because bezier_utils.glsl defines its own make_float2 macro, and
// bezier_utils_test.cpp relies on plain C++ splat casts like float2(2 / 3.f).
#ifdef RIVE_GLSL_CROSS_WANT_CONSTRUCTORS

// Only the fields the shaders' vertex helpers read.
struct GlslCrossUniforms
{
    float renderTargetInverseViewportX;
    float renderTargetInverseViewportY;
};

// These macros are function-like on purpose: they expand where a shader calls
// a constructor and leave declarations -- "INLINE float2 make_float2(...)" --
// alone, since no parenthesis follows the type name there.
#define float2(a, b) make_float2(a, b)
#define float4(a, b, c, d) make_float4(a, b, c, d)
// cast_uint4_to_half4() and friends go through vec4() under GLSL, which the
// tests define so they get those casts rather than the C-style vector casts of
// the other backends. (MSVC's gvec polyfill is a struct, so it can't
// C-style-cast between vector types the way clang's native vectors can.)
#define vec4(a) make_float4_from(a)
#define bool2(a, b) make_bool2(a, b)
#define float2x2(a, b) make_float2x2(a, b)

// Invoke this inside the namespace that includes the shader. It can't live at
// file scope: common.glsl declares its own single-argument splat overloads of
// make_float2() and friends, and these arities have to join that same overload
// set rather than be hidden by it.
#define RIVE_GLSL_CROSS_CONSTRUCTORS                                           \
    [[maybe_unused]] static GlslCrossUniforms uniforms;                        \
    inline float2 make_float2(float x, float y) { return float2{x, y}; }       \
    inline float4 make_float4(float x, float y, float z, float w)              \
    {                                                                          \
        return float4{x, y, z, w};                                             \
    }                                                                          \
    inline bool2 make_bool2(bool a, bool b)                                    \
    {                                                                          \
        return bool2{a ? -1 : 0, b ? -1 : 0};                                  \
    }                                                                          \
    inline float2x2 make_float2x2(float2 a, float2 b)                          \
    {                                                                          \
        return float2x2{a, b};                                                 \
    }                                                                          \
    template <typename T> inline float4 make_float4_from(T v)                  \
    {                                                                          \
        return float4{static_cast<float>(v.x),                                 \
                      static_cast<float>(v.y),                                 \
                      static_cast<float>(v.z),                                 \
                      static_cast<float>(v.w)};                                \
    }

#endif // RIVE_GLSL_CROSS_WANT_CONSTRUCTORS
