/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_MATH_TYPES_DEFINED_
#define _RIVE_MATH_TYPES_DEFINED_

#include "rive/rive_types.hpp"
#include <cmath>

namespace rive {

namespace math {
constexpr float PI = 3.14159265f;
}

template <typename T> T lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }

} // namespace rive

#endif
