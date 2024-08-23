/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from skia:src/core/SkMatrix.cpp
 *
 * Copyright 2022 Rive
 */

#include "rive/math/mat2d.hpp"

#include "rive/math/math_types.hpp"
#include <cmath>

static inline float sdot(float a, float b, float c, float d) { return a * b + c * d; }

namespace rive
{
float Mat2D::findMaxScale() const
{
    if (xy() == 0 && yx() == 0)
    {
        return std::max(fabsf(xx()), fabsf(yy()));
    }

    // ignore the translation part of the matrix, just look at 2x2 portion.
    // compute singular values, take largest or smallest abs value.
    // [a b; b c] = A^T*A
    float a = sdot(xx(), xx(), xy(), xy());
    float b = sdot(xx(), yx(), yy(), xy());
    float c = sdot(yx(), yx(), yy(), yy());
    // eigenvalues of A^T*A are the squared singular values of A.
    // characteristic equation is det((A^T*A) - l*I) = 0
    // l^2 - (a + c)l + (ac-b^2)
    // solve using quadratic equation (divisor is non-zero since l^2 has 1 coeff
    // and roots are guaranteed to be pos and real).
    float bSqd = b * b;
    // if upper left 2x2 is orthogonal save some math
    float result;
    if (bSqd <= math::EPSILON * math::EPSILON)
    {
        result = std::max(a, c);
    }
    else
    {
        float aminusc = a - c;
        float apluscdiv2 = (a + c) * .5f;
        float x = sqrtf(aminusc * aminusc + 4 * bSqd) * .5f;
        result = apluscdiv2 + x;
    }
    if (!std::isfinite(result))
    {
        result = 0;
    }
    // Due to the floating point inaccuracy, there might be an error in a, b, c
    // calculated by sdot, further deepened by subsequent arithmetic operations
    // on them. Therefore, we allow and cap the nearly-zero negative values.
    result = std::max(result, 0.f);
    result = sqrtf(result);
    return result;
}
} // namespace rive
