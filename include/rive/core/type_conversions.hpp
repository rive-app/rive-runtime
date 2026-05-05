/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_TYPE_CONVERSIONS_HPP_
#define _RIVE_TYPE_CONVERSIONS_HPP_

#include "rive/rive_types.hpp"
#include <limits>
#include <type_traits>

namespace rive
{

template <typename T> bool fitsIn(intmax_t x)
{
    return x >= std::numeric_limits<T>::min() &&
           x <= std::numeric_limits<T>::max();
}

template <typename T> T castTo(intmax_t x)
{
    assert(sizeof(T) <= 4); // don't cast to 64bit types
    assert(fitsIn<T>(x));
    return static_cast<T>(x);
}

// Computes a * b for unsigned integral T. Returns true on success and writes
// the product to *out. Returns false if the multiplication overflows; *out is
// then the low bits of the wrapped product (matching __builtin_mul_overflow's
// contract) and must not be relied on by callers.
template <typename T> bool checkedMul(T a, T b, T* out)
{
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                  "checkedMul requires unsigned integer types");
#if __has_builtin(__builtin_mul_overflow)
    return !__builtin_mul_overflow(a, b, out);
#else
    // Widen narrow T to `unsigned` before multiplying so the operation is
    // an unsigned multiplication rather than a signed-int multiplication
    // after integer-promotion (which would be UB on overflow for
    // uint8_t / uint16_t).
    using MulT = typename std::
        conditional<(sizeof(T) < sizeof(unsigned)), unsigned, T>::type;
    const bool overflow = (b != 0 && a > std::numeric_limits<T>::max() / b);
    *out = static_cast<T>(static_cast<MulT>(a) * static_cast<MulT>(b));
    return !overflow;
#endif
}

template <typename T> bool mulOverflows(T a, T b)
{
    T tmp;
    return !checkedMul<T>(a, b, &tmp);
}

} // namespace rive

#endif
