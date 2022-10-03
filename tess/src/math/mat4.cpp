#include "rive/math/mat4.hpp"
#include "rive/math/mat2d.hpp"
#include <cmath>

using namespace rive;

Mat4::Mat4(const Mat2D& mat2d) :
    m_Buffer{
        // clang-format off
        mat2d[0], mat2d[1], 0.0f, 0.0f,
        mat2d[2], mat2d[3], 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 
        mat2d[4], mat2d[5], 0.0f, 1.0f,
        // clang-format on
    }
{}

Mat4 Mat4::multiply(const Mat4& a, const Mat4& b)
{
    return {
        // clang-format off
        b[0] * a[0] + b[1] * a[4] + b[2] * a[8] + b[3] * a[12],
        b[0] * a[1] + b[1] * a[5] + b[2] * a[9] + b[3] * a[13],
        b[0] * a[2] + b[1] * a[6] + b[2] * a[10] + b[3] * a[14],
        b[0] * a[3] + b[1] * a[7] + b[2] * a[11] + b[3] * a[15],

        b[4] * a[0] + b[5] * a[4] + b[6] * a[8] + b[7] * a[12],
        b[4] * a[1] + b[5] * a[5] + b[6] * a[9] + b[7] * a[13],
        b[4] * a[2] + b[5] * a[6] + b[6] * a[10] + b[7] * a[14],
        b[4] * a[3] + b[5] * a[7] + b[6] * a[11] + b[7] * a[15],

        b[8] * a[0] + b[9] * a[4] + b[10] * a[8] + b[11] * a[12],
        b[8] * a[1] + b[9] * a[5] + b[10] * a[9] + b[11] * a[13],
        b[8] * a[2] + b[9] * a[6] + b[10] * a[10] + b[11] * a[14],
        b[8] * a[3] + b[9] * a[7] + b[10] * a[11] + b[11] * a[15],

        b[12] * a[0] + b[13] * a[4] + b[14] * a[8] + b[15] * a[12],
        b[12] * a[1] + b[13] * a[5] + b[14] * a[9] + b[15] * a[13],
        b[12] * a[2] + b[13] * a[6] + b[14] * a[10] + b[15] * a[14],
        b[12] * a[3] + b[13] * a[7] + b[14] * a[11] + b[15] * a[15],
        // clang-format on
    };
}

Mat4 Mat4::multiply(const Mat4& a, const Mat2D& b)
{
    return {
        // clang-format off
        b[0] * a[0] + b[1] * a[4],
        b[0] * a[1] + b[1] * a[5],
        b[0] * a[2] + b[1] * a[6],
        b[0] * a[3] + b[1] * a[7],

        b[2] * a[0] + b[3] * a[4],
        b[2] * a[1] + b[3] * a[5],
        b[2] * a[2] + b[3] * a[6],
        b[2] * a[3] + b[3] * a[7],

        a[8],
        a[9],
        a[10],
        a[11],

        b[4] * a[0] + b[5] * a[4] + a[12],
        b[4] * a[1] + b[5] * a[5] + a[13],
        b[4] * a[2] + b[5] * a[6] + a[14],
        b[4] * a[3] + b[5] * a[7] + a[15],
        // clang-format on
    };
}
