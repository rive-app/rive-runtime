#ifndef _RIVE_MAT4_HPP_
#define _RIVE_MAT4_HPP_

#include "rive/math/simd.hpp"
#include "rive/math/vec2d.hpp"
#include <array>
#include <cmath>
#include <cstddef>

namespace rive
{
// Column-major 4x4 single-precision matrix. The 64-byte storage can be
// uploaded directly to a GPU uniform buffer.
//
// Column 0 = m[0..3], Column 1 = m[4..7], Column 2 = m[8..11], Column 3 =
// m[12..15].
class Mat4
{
public:
    constexpr Mat4() :
        m_buffer{{1.f,
                  0.f,
                  0.f,
                  0.f,
                  0.f,
                  1.f,
                  0.f,
                  0.f,
                  0.f,
                  0.f,
                  1.f,
                  0.f,
                  0.f,
                  0.f,
                  0.f,
                  1.f}}
    {}

    constexpr Mat4(float c0x,
                   float c0y,
                   float c0z,
                   float c0w,
                   float c1x,
                   float c1y,
                   float c1z,
                   float c1w,
                   float c2x,
                   float c2y,
                   float c2z,
                   float c2w,
                   float c3x,
                   float c3y,
                   float c3z,
                   float c3w) :
        m_buffer{{c0x,
                  c0y,
                  c0z,
                  c0w,
                  c1x,
                  c1y,
                  c1z,
                  c1w,
                  c2x,
                  c2y,
                  c2z,
                  c2w,
                  c3x,
                  c3y,
                  c3z,
                  c3w}}
    {}

    const float* values() const { return m_buffer.data(); }
    float* values() { return m_buffer.data(); }

    float& operator[](size_t i) { return m_buffer[i]; }
    float operator[](size_t i) const { return m_buffer[i]; }

    static Mat4 identity() { return Mat4(); }

    static Mat4 fromTranslation(float x, float y, float z)
    {
        Mat4 m;
        m.m_buffer[12] = x;
        m.m_buffer[13] = y;
        m.m_buffer[14] = z;
        return m;
    }

    static Mat4 fromScale(float sx, float sy, float sz)
    {
        Mat4 m;
        m.m_buffer[0] = sx;
        m.m_buffer[5] = sy;
        m.m_buffer[10] = sz;
        return m;
    }

    static Mat4 fromRotationX(float rad)
    {
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 m;
        m.m_buffer[5] = c;
        m.m_buffer[6] = s;
        m.m_buffer[9] = -s;
        m.m_buffer[10] = c;
        return m;
    }

    static Mat4 fromRotationY(float rad)
    {
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 m;
        m.m_buffer[0] = c;
        m.m_buffer[2] = -s;
        m.m_buffer[8] = s;
        m.m_buffer[10] = c;
        return m;
    }

    static Mat4 fromRotationZ(float rad)
    {
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 m;
        m.m_buffer[0] = c;
        m.m_buffer[1] = s;
        m.m_buffer[4] = -s;
        m.m_buffer[5] = c;
        return m;
    }

    // Right-handed perspective. Maps view-space z=[-near, -far] to NDC z in
    // either [0, 1] (default, depthZeroToOne=true) or [-1, 1].
    static Mat4 perspective(float fovYRadians,
                            float aspect,
                            float near_,
                            float far_,
                            bool depthZeroToOne = true)
    {
        float f = 1.f / std::tan(fovYRadians * 0.5f);
        float nf = 1.f / (near_ - far_);
        Mat4 m{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        m.m_buffer[0] = f / aspect;
        m.m_buffer[5] = f;
        if (depthZeroToOne)
        {
            m.m_buffer[10] = far_ * nf;
            m.m_buffer[14] = far_ * near_ * nf;
        }
        else
        {
            m.m_buffer[10] = (far_ + near_) * nf;
            m.m_buffer[14] = 2.f * far_ * near_ * nf;
        }
        m.m_buffer[11] = -1.f;
        return m;
    }

    // SIMD: out = lhs * rhs. Both column-major.
    static Mat4 multiply(const Mat4& lhs, const Mat4& rhs)
    {
        // Each output column j is a linear combination of lhs's columns
        // weighted by rhs's column j.
        const float* L = lhs.m_buffer.data();
        const float* R = rhs.m_buffer.data();
        float4 c0 = simd::load4f(L);
        float4 c1 = simd::load4f(L + 4);
        float4 c2 = simd::load4f(L + 8);
        float4 c3 = simd::load4f(L + 12);

        Mat4 out;
        for (int j = 0; j < 4; ++j)
        {
            const float* rcol = R + j * 4;
            float4 result =
                c0 * rcol[0] + c1 * rcol[1] + c2 * rcol[2] + c3 * rcol[3];
            simd::store(out.m_buffer.data() + j * 4, result);
        }
        return out;
    }

    Mat4 operator*(const Mat4& rhs) const { return multiply(*this, rhs); }

    // SIMD: out = M * (x, y, z, w). Returns a 4-component vector (xyzw).
    void transformVec4(float out[4], float x, float y, float z, float w) const
    {
        float4 c0 = simd::load4f(m_buffer.data());
        float4 c1 = simd::load4f(m_buffer.data() + 4);
        float4 c2 = simd::load4f(m_buffer.data() + 8);
        float4 c3 = simd::load4f(m_buffer.data() + 12);
        simd::store(out, c0 * x + c1 * y + c2 * z + c3 * w);
    }

    Mat4 transposed() const
    {
        Mat4 t;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                t.m_buffer[r * 4 + c] = m_buffer[c * 4 + r];
        return t;
    }

    // Returns true and writes inverse if invertible. Otherwise returns false
    // and `result` is unchanged. Cofactor method.
    bool invert(Mat4* result) const
    {
        const float* m = m_buffer.data();
        float inv[16];

        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
                 m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
                 m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
                 m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
                 m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
                 m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
                 m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
                  m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
                  m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
                 m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
                 m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
                 m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
                 m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
                 m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
                 m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
                  m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
                  m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
                 m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
                 m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
                 m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
                 m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
                  m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
                  m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
                  m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
                  m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
                 m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
                 m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
                 m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
                 m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
                  m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
                  m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
                  m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
                  m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float det =
            m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if (det == 0.f)
            return false;
        float invDet = 1.f / det;
        for (int i = 0; i < 16; ++i)
            (*result)[i] = inv[i] * invDet;
        return true;
    }

private:
    std::array<float, 16> m_buffer;
};

static_assert(std::is_trivially_destructible<Mat4>::value,
              "Mat4 must be trivially destructible");
static_assert(sizeof(Mat4) == 16 * sizeof(float),
              "Mat4 must be 64 bytes (no padding)");

inline bool operator==(const Mat4& a, const Mat4& b)
{
    for (size_t i = 0; i < 16; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}
inline bool operator!=(const Mat4& a, const Mat4& b) { return !(a == b); }

} // namespace rive
#endif
