/*
 * Copyright 2022 Rive
 */

// This header provides a fallback gvec<> implementation for when we don't have gcc/clang vector
// extensions. Swizzles are implemented as unions, which is questionably undefined due to the
// "active member" C++ restriction on unions, however, since the members all have the same
// underlying type, this is a gray area. See:
//
// https://stackoverflow.com/questions/11373203/accessing-inactive-union-member-and-undefined-behavior
//
// This works in Visual Studio, which is the main reason for having this header.

#ifndef _RIVE_SIMD_GVEC_POLYFILL_HPP_
#define _RIVE_SIMD_GVEC_POLYFILL_HPP_

#include <algorithm>
#include <initializer_list>
#include <stdint.h>
#include <cstring>

namespace rive
{
namespace simd
{
using Swizzle = uint32_t;
constexpr static Swizzle PackSwizzle2(uint32_t sourceVectorLength, uint32_t i0, uint32_t i1)
{
    return (i1 << 5) | (i0 << 3) | sourceVectorLength;
}
constexpr static Swizzle PackSwizzle4(uint32_t sourceVectorLength,
                                      uint32_t i0,
                                      uint32_t i1,
                                      uint32_t i2,
                                      uint32_t i3)
{
    return (i3 << 9) | (i2 << 7) | PackSwizzle2(sourceVectorLength, i0, i1);
}
constexpr static uint32_t UnpackSwizzleSourceVectorLength(Swizzle swizzle) { return swizzle & 7; }
constexpr static uint32_t UnpackSwizzleIdx(Swizzle swizzle, uint32_t i)
{
    return (swizzle >> (i * 2 + 3)) & 3;
}

template <typename T, int N, Swizzle Z = 0> struct gvec
{
    T operator[](size_t i) const { return data[UnpackSwizzleIdx(Z, i)]; }
    T& operator[](size_t i) { return data[UnpackSwizzleIdx(Z, i)]; }
    operator gvec<T, N>() const
    {
        gvec<T, N> ret;
        for (int i = 0; i < N; ++i)
            ret[i] = (*this)[i];
        return ret;
    }
    T data[UnpackSwizzleSourceVectorLength(Z)];
};

template <typename T, int N> struct gvec_data
{
    T data[N];
};

template <typename T> struct gvec_data<T, 1>
{
    union
    {
        T data[1];
        T x;
    };
};

template <typename T> struct gvec_data<T, 2>
{
    union
    {
        T data[2];
        struct
        {
            T x, y;
        };
        gvec<T, 2, PackSwizzle2(2, 1, 0)> yx;
        gvec<T, 4, PackSwizzle4(2, 0, 1, 0, 1)> xyxy;
        gvec<T, 4, PackSwizzle4(2, 1, 0, 1, 0)> yxyx;
    };
};

template <typename T> struct gvec_data<T, 3>
{
    union
    {
        T data[2];
        struct
        {
            T x, y, z;
        };
    };
};

template <typename T> struct gvec_data<T, 4>
{
    union
    {
        T data[4];
        gvec<T, 3> xyz;
        struct
        {
            gvec<T, 2> xy, zw;
        };
        struct
        {
            T x;
            union
            {
                gvec<T, 3> yzw;
                gvec<T, 2> yz;
                struct
                {
                    T y, z, w;
                };
            };
        };
        // **WARNING**!! Only add swizzles that include ALL components of the vector. Since these
        // types are POD, it's not possible to overwrite their default operator=, and their default
        // operator= is just a memcpy. So: "float.xz = float4.xz" would also assign y and w.
        gvec<T, 4, PackSwizzle4(4, 1, 0, 3, 2)> yxwz;
        gvec<T, 4, PackSwizzle4(4, 2, 3, 0, 1)> zwxy;
        gvec<T, 4, PackSwizzle4(4, 2, 1, 0, 3)> zyxw;
        gvec<T, 4, PackSwizzle4(4, 0, 3, 2, 1)> xwzy;
    };
};

template <typename T, int N> struct gvec<T, N, 0> : public gvec_data<T, N>
{
    gvec() = default;
    gvec(T val)
    {
        for (int i = 0; i < N; ++i)
            gvec_data<T, N>::data[i] = val;
    }
    gvec(std::initializer_list<T> vals)
    {
        memset(gvec_data<T, N>::data, 0, sizeof(gvec_data<T, N>::data));
        std::copy(vals.begin(),
                  vals.begin() + std::min<size_t>(vals.size(), N),
                  gvec_data<T, N>::data);
    }
    T operator[](size_t i) const { return gvec_data<T, N>::data[i]; }
    T& operator[](size_t i) { return gvec_data<T, N>::data[i]; }
};

static_assert(sizeof(gvec<float, 1>) == 4, "gvec<1> is expected to be tightly packed");
static_assert(sizeof(gvec<float, 2>) == 8, "gvec<2> is expected to be tightly packed");
static_assert(sizeof(gvec<float, 4>) == 16, "gvec<4> is expected to be tightly packed");

// Vector booleans are masks of integer type, where true is -1 and false is 0. Vector booleans masks
// are generated using the builtin boolean operators: ==, !=, <=, >=, <, >
template <size_t> struct boolean_mask_type_by_size;
template <> struct boolean_mask_type_by_size<1>
{
    using type = int8_t;
};
template <> struct boolean_mask_type_by_size<2>
{
    using type = int16_t;
};
template <> struct boolean_mask_type_by_size<4>
{
    using type = int32_t;
};
template <> struct boolean_mask_type_by_size<8>
{
    using type = int64_t;
};
template <typename T> struct boolean_mask_type
{
    using type = typename boolean_mask_type_by_size<sizeof(T)>::type;
};

#define DECL_UNARY_OP(_OP_)                                                                        \
    template <typename T, int N, Swizzle Z> gvec<T, N> operator _OP_(gvec<T, N, Z> x)              \
    {                                                                                              \
        gvec<T, N> ret;                                                                            \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = _OP_ x[i];                                                                    \
        return ret;                                                                                \
    }

DECL_UNARY_OP(+)
DECL_UNARY_OP(-)
DECL_UNARY_OP(~)

#undef DECL_UNARY_OP

#define DECL_ARITHMETIC_OP(_OP_)                                                                   \
    template <typename T, typename U, int N, Swizzle Z0, Swizzle Z1>                               \
    gvec<T, N, Z0>& operator _OP_##=(gvec<T, N, Z0>& a, gvec<U, N, Z1> b)                          \
    {                                                                                              \
        for (int i = 0; i < N; ++i)                                                                \
            a[i] _OP_## = b[i];                                                                    \
        return a;                                                                                  \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z>                                            \
    gvec<T, N, Z>& operator _OP_##=(gvec<T, N, Z>& a, U b)                                         \
    {                                                                                              \
        for (int i = 0; i < N; ++i)                                                                \
            a[i] _OP_## = b;                                                                       \
        return a;                                                                                  \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z0, Swizzle Z1>                               \
    gvec<T, N> operator _OP_(gvec<T, N, Z0> a, gvec<U, N, Z1> b)                                   \
    {                                                                                              \
        gvec<T, N> ret;                                                                            \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a[i] _OP_ b[i];                                                               \
        return ret;                                                                                \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z>                                            \
    gvec<T, N> operator _OP_(gvec<T, N, Z> a, U b)                                                 \
    {                                                                                              \
        gvec<T, N> ret;                                                                            \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a[i] _OP_ b;                                                                  \
        return ret;                                                                                \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z>                                            \
    gvec<U, N> operator _OP_(T a, gvec<U, N, Z> b)                                                 \
    {                                                                                              \
        gvec<T, N> ret;                                                                            \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a _OP_ b[i];                                                                  \
        return ret;                                                                                \
    }

DECL_ARITHMETIC_OP(+);
DECL_ARITHMETIC_OP(-);
DECL_ARITHMETIC_OP(*);
DECL_ARITHMETIC_OP(/);
DECL_ARITHMETIC_OP(%);
DECL_ARITHMETIC_OP(|);
DECL_ARITHMETIC_OP(&);
DECL_ARITHMETIC_OP(^);
DECL_ARITHMETIC_OP(<<);
DECL_ARITHMETIC_OP(>>);

#undef DECL_ARITHMETIC_OP

#define DECL_BOOLEAN_OP(_OP_)                                                                      \
    template <typename T, int N, Swizzle Z0, Swizzle Z1>                                           \
    gvec<typename boolean_mask_type<T>::type, N> operator _OP_(gvec<T, N, Z0> a, gvec<T, N, Z1> b) \
    {                                                                                              \
        gvec<typename boolean_mask_type<T>::type, N> ret;                                          \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a[i] _OP_ b[i] ? ~0 : 0;                                                      \
        return ret;                                                                                \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z>                                            \
    gvec<typename boolean_mask_type<T>::type, N> operator _OP_(gvec<T, N, Z> a, U b)               \
    {                                                                                              \
        gvec<typename boolean_mask_type<T>::type, N> ret;                                          \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a[i] _OP_ b ? ~0 : 0;                                                         \
        return ret;                                                                                \
    }                                                                                              \
    template <typename T, typename U, int N, Swizzle Z>                                            \
    gvec<typename boolean_mask_type<T>::type, N> operator _OP_(U a, gvec<T, N, Z> b)               \
    {                                                                                              \
        gvec<typename boolean_mask_type<T>::type, N> ret;                                          \
        for (int i = 0; i < N; ++i)                                                                \
            ret[i] = a _OP_ b[i] ? ~0 : 0;                                                         \
        return ret;                                                                                \
    }

DECL_BOOLEAN_OP(==)
DECL_BOOLEAN_OP(!=)
DECL_BOOLEAN_OP(<)
DECL_BOOLEAN_OP(<=)
DECL_BOOLEAN_OP(>)
DECL_BOOLEAN_OP(>=)
DECL_BOOLEAN_OP(&&)
DECL_BOOLEAN_OP(||)

#undef DECL_BOOLEAN_OP

#define ENABLE_SWIZZLE1(F)                                                                         \
    template <typename T, int N, Swizzle Z0> gvec<T, N> F(gvec<T, N, Z0> x)                        \
    {                                                                                              \
        return F((gvec<T, N>)x);                                                                   \
    }
#define ENABLE_SWIZZLE_REDUCE(F)                                                                   \
    template <typename T, int N, Swizzle Z0> T F(gvec<T, N, Z0> x) { return F((gvec<T, N>)x); }
#define ENABLE_SWIZZLE1F(F)                                                                        \
    template <int N, Swizzle Z0> gvec<float, N> F(gvec<float, N, Z0> x)                            \
    {                                                                                              \
        return F((gvec<float, N>)x);                                                               \
    }
#define ENABLE_SWIZZLE1B(F)                                                                        \
    template <typename T, int N, Swizzle Z0> bool F(gvec<T, N, Z0> x) { return F((gvec<T, N>)x); }
#define ENABLE_SWIZZLEUT(F)                                                                        \
    template <typename T, typename U, int N, Swizzle Z0> gvec<U, N> F(gvec<T, N, Z0> x)            \
    {                                                                                              \
        return F((gvec<T, N>)x);                                                                   \
    }
#define ENABLE_SWIZZLE2(F)                                                                         \
    template <typename T, int N, Swizzle Z0, Swizzle Z1>                                           \
    gvec<T, N> F(gvec<T, N, Z0> a, gvec<T, N, Z1> b)                                               \
    {                                                                                              \
        return F((gvec<T, N>)a, (gvec<T, N>)b);                                                    \
    }
#define ENABLE_SWIZZLE3(F)                                                                         \
    template <typename T, int N, Swizzle Z0, Swizzle Z1, Swizzle Z2>                               \
    gvec<T, N> F(gvec<T, N, Z0> a, gvec<T, N, Z1> b, gvec<T, N, Z2> c)                             \
    {                                                                                              \
        return F((gvec<T, N>)a, (gvec<T, N>)b, (gvec<T, N>)c);                                     \
    }
#define ENABLE_SWIZZLE3F(F)                                                                        \
    template <int N, Swizzle Z0, Swizzle Z1, Swizzle Z2>                                           \
    gvec<float, N> F(gvec<float, N, Z0> a, gvec<float, N, Z1> b, gvec<float, N, Z2> c)             \
    {                                                                                              \
        return F((gvec<float, N>)a, (gvec<float, N>)b, (gvec<float, N>)c);                         \
    }
#define ENABLE_SWIZZLE3IT(F)                                                                       \
    template <typename T, int N, Swizzle Z0, Swizzle Z1, Swizzle Z2>                               \
    gvec<T, N> F(gvec<typename boolean_mask_type<T>::type, N, Z0> a,                               \
                 gvec<T, N, Z1> b,                                                                 \
                 gvec<T, N, Z2> c)                                                                 \
    {                                                                                              \
        return F((gvec<int32_t, N>)a, (gvec<T, N>)b, (gvec<T, N>)c);                               \
    }

ENABLE_SWIZZLE1(abs)
ENABLE_SWIZZLE_REDUCE(reduce_add)
ENABLE_SWIZZLE_REDUCE(reduce_min)
ENABLE_SWIZZLE_REDUCE(reduce_max)
ENABLE_SWIZZLE_REDUCE(reduce_and)
ENABLE_SWIZZLE_REDUCE(reduce_or)
ENABLE_SWIZZLE1F(floor)
ENABLE_SWIZZLE1F(ceil)
ENABLE_SWIZZLE1F(sqrt)
ENABLE_SWIZZLE1F(fast_acos)
ENABLE_SWIZZLE1B(any)
ENABLE_SWIZZLE1B(all)
ENABLE_SWIZZLE2(min)
ENABLE_SWIZZLE2(max)
ENABLE_SWIZZLE3(clamp)
ENABLE_SWIZZLE3F(mix)
ENABLE_SWIZZLE3F(precise_mix)
ENABLE_SWIZZLE3IT(if_then_else)
template <typename T, int N, Swizzle Z> void store(void* dst, gvec<T, N, Z> vec)
{
    store(dst, (gvec<T, N>)vec);
}
template <typename U, typename T, int N, Swizzle Z> gvec<U, N> cast(gvec<T, N, Z> x)
{
    return cast<U>((gvec<T, N>)x);
}

#undef ENABLE_SWIZZLE1
#undef ENABLE_SWIZZLE_REDUCE
#undef ENABLE_SWIZZLE1F
#undef ENABLE_SWIZZLE1B
#undef ENABLE_SWIZZLEUT
#undef ENABLE_SWIZZLE2
#undef ENABLE_SWIZZLE3
#undef ENABLE_SWIZZLE3F
#undef ENABLE_SWIZZLE3IT
} // namespace simd
} // namespace rive

#endif
