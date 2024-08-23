/*
 * Copyright 2022 Rive
 */

#include "rive/math/math_types.hpp"
#include "rive/math/raw_path_utils.hpp"
#include <cmath>

// Extract subsets

void rive::quad_subdivide(const rive::Vec2D src[3], float t, rive::Vec2D dst[5])
{
    assert(t >= 0 && t <= 1);
    auto ab = lerp(src[0], src[1], t);
    auto bc = lerp(src[1], src[2], t);
    dst[0] = src[0];
    dst[1] = ab;
    dst[2] = lerp(ab, bc, t);
    dst[3] = bc;
    dst[4] = src[2];
}

void rive::cubic_subdivide(const rive::Vec2D src[4], float t, rive::Vec2D dst[7])
{
    assert(t >= 0 && t <= 1);
    auto ab = lerp(src[0], src[1], t);
    auto bc = lerp(src[1], src[2], t);
    auto cd = lerp(src[2], src[3], t);
    auto abc = lerp(ab, bc, t);
    auto bcd = lerp(bc, cd, t);
    dst[0] = src[0];
    dst[1] = ab;
    dst[2] = abc;
    dst[3] = lerp(abc, bcd, t);
    dst[4] = bcd;
    dst[5] = cd;
    dst[6] = src[3];
}

void rive::line_extract(const rive::Vec2D src[2], float startT, float endT, rive::Vec2D dst[2])
{
    assert(startT <= endT);
    assert(startT >= 0 && endT <= 1);

    dst[0] = lerp(src[0], src[1], startT);
    dst[1] = lerp(src[0], src[1], endT);
}

void rive::quad_extract(const rive::Vec2D src[3], float startT, float endT, rive::Vec2D dst[3])
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
        rive::quad_subdivide(src, endT, tmp);
        std::copy(tmp, tmp + 3, dst);
    }
    else if (endT == 1)
    {
        rive::quad_subdivide(src, startT, tmp);
        std::copy(tmp + 2, tmp + 5, dst);
    }
    else
    {
        assert(endT > 0);
        rive::quad_subdivide(src, endT, tmp);
        rive::Vec2D tmp2[5];
        rive::quad_subdivide(tmp, startT / endT, tmp2);
        std::copy(tmp2 + 2, tmp2 + 5, dst);
    }
}

void rive::cubic_extract(const rive::Vec2D src[4], float startT, float endT, rive::Vec2D dst[4])
{
    assert(startT <= endT);
    assert(startT >= 0 && endT <= 1);

    rive::Vec2D tmp[7];
    if (startT == 0 && endT == 1)
    {
        std::copy(src, src + 4, dst);
    }
    else if (startT == 0)
    {
        rive::cubic_subdivide(src, endT, tmp);
        std::copy(tmp, tmp + 4, dst);
    }
    else if (endT == 1)
    {
        rive::cubic_subdivide(src, startT, tmp);
        std::copy(tmp + 3, tmp + 7, dst);
    }
    else
    {
        assert(endT > 0);
        rive::cubic_subdivide(src, endT, tmp);
        rive::Vec2D tmp2[7];
        rive::cubic_subdivide(tmp, startT / endT, tmp2);
        std::copy(tmp2 + 3, tmp2 + 7, dst);
    }
}
