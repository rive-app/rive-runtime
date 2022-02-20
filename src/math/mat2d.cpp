#include "rive/math/mat2d.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/math/vec2d.hpp"
#include <cmath>

using namespace rive;

Mat2D Mat2D::fromRotation(float rad) {
    float s = 0,
          c = 1;
    if (rad != 0) {
        s = sin(rad);
        c = cos(rad);
    }
    return {c, s, -s, c, 0, 0};
}

Mat2D Mat2D::scale(const Mat2D& mat, const Vec2D& vec) {
    const float v0 = vec[0],
                v1 = vec[1];
    return {
        mat[0] * v0, mat[1] * v0, mat[2] * v1, mat[3] * v1, mat[4], mat[5],
    };
}

Mat2D Mat2D::multiply(const Mat2D& a, const Mat2D& b) {
    float a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4], a5 = a[5],
          b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3], b4 = b[4], b5 = b[5];
    return {
        a0 * b0 + a2 * b1,
        a1 * b0 + a3 * b1,
        a0 * b2 + a2 * b3,
        a1 * b2 + a3 * b3,
        a0 * b4 + a2 * b5 + a4,
        a1 * b4 + a3 * b5 + a5,
    };
}

bool Mat2D::invert(Mat2D& result, const Mat2D& a) {
    float aa = a[0], ab = a[1], ac = a[2], ad = a[3], atx = a[4], aty = a[5];

    float det = aa * ad - ab * ac;
    if (det == 0.0f) {
        return false;
    }
    det = 1.0f / det;

    result[0] = ad * det;
    result[1] = -ab * det;
    result[2] = -ac * det;
    result[3] = aa * det;
    result[4] = (ac * aty - ad * atx) * det;
    result[5] = (ab * atx - aa * aty) * det;
    return true;
}

void Mat2D::decompose(TransformComponents& result, const Mat2D& m) {
    float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];

    float rotation = (float)std::atan2(m1, m0);
    float denom = m0 * m0 + m1 * m1;
    float scaleX = (float)std::sqrt(denom);
    float scaleY = (m0 * m3 - m2 * m1) / scaleX;
    float skewX = (float)std::atan2(m0 * m2 + m1 * m3, denom);

    result.x(m[4]);
    result.y(m[5]);
    result.scaleX(scaleX);
    result.scaleY(scaleY);
    result.rotation(rotation);
    result.skew(skewX);
}

void Mat2D::compose(Mat2D& result, const TransformComponents& components) {
    result = Mat2D::fromRotation(components.rotation());
    result[4] = components.x();
    result[5] = components.y();
    Vec2D scale;
    components.scale(scale);
    result = Mat2D::scale(result, scale);

    float sk = components.skew();
    if (sk != 0.0f) {
        result[2] = result[0] * sk + result[2];
        result[3] = result[1] * sk + result[3];
    }
}

void Mat2D::scaleByValues(float sx, float sy) {
    m_Buffer[0] *= sx;
    m_Buffer[1] *= sx;
    m_Buffer[2] *= sy;
    m_Buffer[3] *= sy;
}
