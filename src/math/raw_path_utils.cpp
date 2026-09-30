/*
 * Copyright 2022 Rive
 */

#include "rive/math/raw_path_utils.hpp"
#include "rive/math/bezier_utils.hpp"
#include <algorithm>

// Exact at t=0, t=1 and when a == b, so coincident points survive a chop.
static rive::Vec2D exact_lerp(rive::Vec2D a, rive::Vec2D b, float t)
{
    return t < 0.5f ? a + (b - a) * t : b - (b - a) * (1 - t);
}

static void quad_subdivide(const rive::Vec2D src[3],
                           float t,
                           rive::Vec2D dst[5])
{
    assert(t >= 0 && t <= 1);
    auto ab = exact_lerp(src[0], src[1], t);
    auto bc = exact_lerp(src[1], src[2], t);
    dst[0] = src[0];
    dst[1] = ab;
    dst[2] = exact_lerp(ab, bc, t);
    dst[3] = bc;
    dst[4] = src[2];
}

void rive::line_extract(const rive::Vec2D src[2],
                        float startT,
                        float endT,
                        rive::Vec2D dst[2])
{
    assert(startT <= endT);
    assert(startT >= 0 && endT <= 1);

    dst[0] = exact_lerp(src[0], src[1], startT);
    dst[1] = exact_lerp(src[0], src[1], endT);
}

void rive::quad_extract(const rive::Vec2D src[3],
                        float startT,
                        float endT,
                        rive::Vec2D dst[3])
{
    assert(startT <= endT);
    assert(startT >= 0 && endT <= 1);

    rive::Vec2D tmp[5];
    if (startT == 0 && endT == 1)
    {
        std::copy(src, src + 3, dst);
    }
    else if (startT == 0)
    {
        quad_subdivide(src, endT, tmp);
        std::copy(tmp, tmp + 3, dst);
    }
    else if (endT == 1)
    {
        quad_subdivide(src, startT, tmp);
        std::copy(tmp + 2, tmp + 5, dst);
    }
    else
    {
        assert(endT > 0);
        quad_subdivide(src, endT, tmp);
        rive::Vec2D tmp2[5];
        quad_subdivide(tmp, startT / endT, tmp2);
        std::copy(tmp2 + 2, tmp2 + 5, dst);
    }
}

void rive::cubic_extract(const rive::Vec2D src[4],
                         float startT,
                         float endT,
                         rive::Vec2D dst[4])
{
    rive::Vec2D tmp[10];
    rive::math::chop_cubic_at(src, tmp, startT, endT);
    std::copy(tmp + 3, tmp + 7, dst);
}
