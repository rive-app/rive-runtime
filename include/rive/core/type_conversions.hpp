/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_TYPE_CONVERSIONS_HPP_
#define _RIVE_TYPE_CONVERSIONS_HPP_

#include "rive/rive_types.hpp"
#include <limits>

namespace rive
{

template <typename T> bool fitsIn(intmax_t x)
{
    return x >= std::numeric_limits<T>::min() && x <= std::numeric_limits<T>::max();
}

template <typename T> T castTo(intmax_t x)
{
    assert(sizeof(T) <= 4); // don't cast to 64bit types
    assert(fitsIn<T>(x));
    return static_cast<T>(x);
}

} // namespace rive

#endif
