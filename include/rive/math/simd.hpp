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

#include <stdint.h>

#define SIMD_ALWAYS_INLINE inline __attribute__((always_inline))

namespace rive
{
namespace simd
{

// The GLSL spec uses "gvec" to denote a vector of unspecified type.
template <typename T, int N>
using gvec = T __attribute__((ext_vector_type(N))) __attribute__((aligned(sizeof(T) * N)));

////// Math //////

// Similar to std::min(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> min(gvec<T, N> a, gvec<T, N> b)
{
    return __builtin_elementwise_min(a, b);
}

// Similar to std::max(), with a noteworthy difference:
// If a[i] or b[i] is NaN and the other is not, returns whichever is _not_ NaN.
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> max(gvec<T, N> a, gvec<T, N> b)
{
    return __builtin_elementwise_max(a, b);
}

// Returns the absolute value of x per element, with one exception:
// If x[i] is an integer type and equal to the minimum representable value, returns x[i].
template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> abs(gvec<T, N> x)
{
    return __builtin_elementwise_abs(x);
}

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

////// Loading and storing //////

template <typename T, int N> SIMD_ALWAYS_INLINE gvec<T, N> load(const T* ptr)
{
    gvec<T, N> vec;
    __builtin_memcpy(&vec, ptr, sizeof(vec));
    return vec;
}
SIMD_ALWAYS_INLINE gvec<float, 2> load2f(const float* ptr) { return load<float, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<float, 4> load4f(const float* ptr) { return load<float, 4>(ptr); }
SIMD_ALWAYS_INLINE gvec<int32_t, 2> load2i(const int32_t* ptr) { return load<int32_t, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<int32_t, 4> load4i(const int32_t* ptr) { return load<int32_t, 4>(ptr); }
SIMD_ALWAYS_INLINE gvec<uint32_t, 2> load2ui(const uint32_t* ptr) { return load<uint32_t, 2>(ptr); }
SIMD_ALWAYS_INLINE gvec<uint32_t, 4> load4ui(const uint32_t* ptr) { return load<uint32_t, 4>(ptr); }

template <typename T, int N> SIMD_ALWAYS_INLINE void store(T* ptr, gvec<T, N> vec)
{
    __builtin_memcpy(ptr, &vec, sizeof(vec));
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
