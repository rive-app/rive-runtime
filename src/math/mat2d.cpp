#include "rive/math/math_types.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/simd.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

Mat2D Mat2D::fromRotation(float rad)
{
    float s = 0, c = 1;
    if (rad != 0)
    {
        s = sin(rad);
        c = cos(rad);
    }
    return {c, s, -s, c, 0, 0};
}

Mat2D Mat2D::scale(Vec2D vec) const
{
    return {
        m_buffer[0] * vec.x,
        m_buffer[1] * vec.x,
        m_buffer[2] * vec.y,
        m_buffer[3] * vec.y,
        m_buffer[4],
        m_buffer[5],
    };
}

Mat2D Mat2D::multiply(const Mat2D& a, const Mat2D& b)
{
    float a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4], a5 = a[5], b0 = b[0], b1 = b[1],
          b2 = b[2], b3 = b[3], b4 = b[4], b5 = b[5];
    return {
        a0 * b0 + a2 * b1,
        a1 * b0 + a3 * b1,
        a0 * b2 + a2 * b3,
        a1 * b2 + a3 * b3,
        a0 * b4 + a2 * b5 + a4,
        a1 * b4 + a3 * b5 + a5,
    };
}

void Mat2D::mapPoints(Vec2D dst[], const Vec2D pts[], size_t n) const
{
    size_t i = 0;
    float4 scale = float2{m_buffer[0], m_buffer[3]}.xyxy;
    float4 skew = simd::load2f(&m_buffer[1]).yxyx;
    float4 trans = simd::load2f(&m_buffer[4]).xyxy;
    if (simd::all(skew.xy == 0.f))
    {
        // Scale + translate matrix.
        if (n & 1)
        {
            float2 p = simd::load2f(pts);
            p = scale.xy * p + trans.xy;
            simd::store(dst, p);
            ++i;
        }
        for (; i < n; i += 2)
        {
            float4 p = simd::load4f(pts + i);
            p = scale * p + trans;
            simd::store(dst + i, p);
        }
    }
    else
    {
        // Affine matrix.
        if (n & 1)
        {
            float2 p = simd::load2f(pts);
            float2 p_ = skew.xy * p.yx + trans.xy;
            p_ = scale.xy * p + p_;
            simd::store(dst, p_);
            ++i;
        }
        for (; i < n; i += 2)
        {
            float4 p = simd::load4f(pts + i);
            float4 p_ = skew * p.yxwz + trans;
            p_ = scale * p + p_;
            simd::store(dst + i, p_);
        }
    }
}

AABB Mat2D::mapBoundingBox(const Vec2D pts[], size_t n) const
{
    size_t i = 0;
    float4 scale = float2{m_buffer[0], m_buffer[3]}.xyxy;
    float4 skew = simd::load2f(&m_buffer[1]).yxyx;
    float4 mins = std::numeric_limits<float>::infinity();
    float4 maxes = -std::numeric_limits<float>::infinity();
    if (simd::all(skew.xy == 0.f))
    {
        // Scale + translate matrix.
        if (n & 1)
        {
            float2 p = simd::load2f(pts);
            p = scale.xy * p;
            mins.xy = maxes.xy = p;
            ++i;
        }
        for (; i < n; i += 2)
        {
            float4 p = simd::load4f(pts + i);
            p = scale * p;
            mins = simd::min(p, mins);
            maxes = simd::max(p, maxes);
        }
    }
    else
    {
        // Affine matrix.
        if (n & 1)
        {
            float2 p = simd::load2f(pts);
            float2 p_ = skew.xy * p.yx;
            p_ = scale.xy * p + p_;
            mins.xy = maxes.xy = p_;
            ++i;
        }
        for (; i < n; i += 2)
        {
            float4 p = simd::load4f(pts + i);
            float4 p_ = skew * p.yxwz;
            p_ = scale * p + p_;
            mins = simd::min(p_, mins);
            maxes = simd::max(p_, maxes);
        }
    }

    float4 bbox = simd::join(simd::min(mins.xy, mins.zw), simd::max(maxes.xy, maxes.zw));
    // Use logic that takes the "nonfinite" branch when bbox has NaN values.
    // Use "b - a >= 0" instead of "a >= b" because it fails when b == a == inf.
    if (!simd::all(bbox.zw - bbox.xy >= 0))
    {
        // The given points were NaN or empty, or infinite.
        bbox = float4(0);
    }
    else
    {
        float4 trans = simd::load2f(&m_buffer[4]).xyxy;
        bbox += trans;
    }

    auto aabb = math::bit_cast<AABB>(bbox);
    assert(aabb.width() >= 0);
    assert(aabb.height() >= 0);
    return aabb;
}

AABB Mat2D::mapBoundingBox(const AABB& aabb) const
{
    Vec2D pts[4] = {{aabb.left(), aabb.top()},
                    {aabb.right(), aabb.top()},
                    {aabb.right(), aabb.bottom()},
                    {aabb.left(), aabb.bottom()}};
    return mapBoundingBox(pts, 4);
}

bool Mat2D::invert(Mat2D* result) const
{
    float aa = m_buffer[0], ab = m_buffer[1], ac = m_buffer[2], ad = m_buffer[3], atx = m_buffer[4],
          aty = m_buffer[5];

    float det = aa * ad - ab * ac;
    if (det == 0.0f)
    {
        return false;
    }
    det = 1.0f / det;

    *result = {
        ad * det,
        -ab * det,
        -ac * det,
        aa * det,
        (ac * aty - ad * atx) * det,
        (ab * atx - aa * aty) * det,
    };
    return true;
}

TransformComponents Mat2D::decompose() const
{
    float m0 = m_buffer[0], m1 = m_buffer[1], m2 = m_buffer[2], m3 = m_buffer[3];

    float rotation = (float)std::atan2(m1, m0);
    float denom = m0 * m0 + m1 * m1;
    float scaleX = (float)std::sqrt(denom);
    float scaleY = scaleX == 0.0f ? 0.0f : (m0 * m3 - m2 * m1) / scaleX;
    float skewX = (float)std::atan2(m0 * m2 + m1 * m3, denom);

    TransformComponents result;
    result.x(m_buffer[4]);
    result.y(m_buffer[5]);
    result.scaleX(scaleX);
    result.scaleY(scaleY);
    result.rotation(rotation);
    result.skew(skewX);
    return result;
}

Mat2D Mat2D::compose(const TransformComponents& components)
{
    auto result = Mat2D::fromRotation(components.rotation());
    result[4] = components.x();
    result[5] = components.y();
    result = result.scale(components.scale());

    float sk = components.skew();
    if (sk != 0.0f)
    {
        result[2] = result[0] * sk + result[2];
        result[3] = result[1] * sk + result[3];
    }
    return result;
}

void Mat2D::scaleByValues(float sx, float sy)
{
    m_buffer[0] *= sx;
    m_buffer[1] *= sx;
    m_buffer[2] *= sy;
    m_buffer[3] *= sy;
}
