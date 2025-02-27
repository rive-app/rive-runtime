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
using float2x2 = std::array<float2, 2>;

inline float clamp(float x, float lo, float hi)
{
    return std::clamp(x, lo, hi);
}
inline float min(float x, float y) { return std::min(x, y); }
inline float max(float x, float y) { return std::max(x, y); }

template <typename T> T inversesqrt(T x) { return 1 / sqrt(x); }
template <int N> float length(vec<N> x) { return std::sqrt(dot(x, x)); }
template <int N> vec<N> normalize(vec<N> x) { return x / length(x); }

template <typename T, int N>
ivec<N> notEqual(simd::gvec<T, N> x, simd::gvec<T, N> y)
{
    return x != y;
}

#endif
