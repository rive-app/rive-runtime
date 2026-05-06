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

    // Right-handed perspective with reverse-Z (near -> 1, far -> 0) and an
    // infinite far plane. Combined with a float depth buffer this gives a
    // near-uniform 1/z depth distribution across the entire frustum — the
    // best precision an arbitrary scene can hope for. See Upchurch & Desbrun,
    // "Tightening the Precision of Perspective Rendering" (2012).
    //
    // Caller's depth buffer must be cleared to 0 (not 1) and the depth test
    // flipped (GREATER, not LESS).
    static Mat4 perspectiveReverseZ(float fovYRadians,
                                    float aspect,
                                    float near_)
    {
        float f = 1.f / std::tan(fovYRadians * 0.5f);
        Mat4 m{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        m.m_buffer[0] = f / aspect;
        m.m_buffer[5] = f;
        m.m_buffer[10] = 0.f;
        m.m_buffer[11] = -1.f;
        m.m_buffer[14] = near_;
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

    // SIMD: out = lhs * rhs, assuming both are affine (bottom row
    // [0, 0, 0, 1]). Skips the four FMAs that would multiply lhs's bottom-
    // row zeros and the rhs[3]=0 entries of the first three columns.
    //
    // Result is always affine. Passing a non-affine input gives an
    // incorrect result — callers must ensure the contract.
    static Mat4 multiplyAffine(const Mat4& lhs, const Mat4& rhs)
    {
        const float* L = lhs.m_buffer.data();
        const float* R = rhs.m_buffer.data();
        float4 c0 = simd::load4f(L);      // [_,_,_,0]
        float4 c1 = simd::load4f(L + 4);  // [_,_,_,0]
        float4 c2 = simd::load4f(L + 8);  // [_,_,_,0]
        float4 c3 = simd::load4f(L + 12); // [_,_,_,1]

        Mat4 out;
        // Cols 0..2: rhs[3] is 0, so the c3*rhs[3] term vanishes.
        for (int j = 0; j < 3; ++j)
        {
            const float* rcol = R + j * 4;
            float4 result = c0 * rcol[0] + c1 * rcol[1] + c2 * rcol[2];
            simd::store(out.m_buffer.data() + j * 4, result);
        }
        // Col 3: rhs[3] is 1, so c3 contributes directly.
        {
            const float* rcol = R + 12;
            float4 result = c0 * rcol[0] + c1 * rcol[1] + c2 * rcol[2] + c3;
            simd::store(out.m_buffer.data() + 12, result);
        }
        return out;
    }

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

    // Closed-form inverse for affine matrices (bottom row [0, 0, 0, 1]).
    // Inverts the 3x3 linear part R via cofactors, then writes -R^-1 * t
    // into the translation column. Much smaller and faster than the full
    // 4x4 cofactor `invert`.
    //
    // Returns false (and leaves `result` unchanged) only if the linear part
    // is singular. Caller must ensure the input is actually affine.
    bool invertAffine(Mat4* result) const
    {
        const float* m = m_buffer.data();
        // The 3x3 linear part R has R[row][col] = m[col*4 + row].
        // Cofactors C[i][j] of column 0 of R, expanded for det along col 0.
        float c00 = m[5] * m[10] - m[6] * m[9];
        float c10 = m[6] * m[8] - m[4] * m[10];
        float c20 = m[4] * m[9] - m[5] * m[8];
        float det = m[0] * c00 + m[1] * c10 + m[2] * c20;
        if (det == 0.f)
            return false;
        float invDet = 1.f / det;
        // Remaining 6 cofactors.
        float c01 = m[2] * m[9] - m[1] * m[10];
        float c02 = m[1] * m[6] - m[2] * m[5];
        float c11 = m[0] * m[10] - m[2] * m[8];
        float c12 = m[2] * m[4] - m[0] * m[6];
        float c21 = m[1] * m[8] - m[0] * m[9];
        float c22 = m[0] * m[5] - m[1] * m[4];

        // R^-1 = (cofactor matrix)^T / det, so Rinv[i][j] = C[j][i] / det.
        // Naming below: ri_j = Rinv[i][j].
        float r0_0 = c00 * invDet, r0_1 = c10 * invDet, r0_2 = c20 * invDet;
        float r1_0 = c01 * invDet, r1_1 = c11 * invDet, r1_2 = c21 * invDet;
        float r2_0 = c02 * invDet, r2_1 = c12 * invDet, r2_2 = c22 * invDet;

        // Translation column: -R^-1 * t.
        float tx = m[12], ty = m[13], tz = m[14];
        float ix = -(r0_0 * tx + r0_1 * ty + r0_2 * tz);
        float iy = -(r1_0 * tx + r1_1 * ty + r1_2 * tz);
        float iz = -(r2_0 * tx + r2_1 * ty + r2_2 * tz);

        // Store column-major: o[col*4 + row] = Rinv[row][col].
        float* o = result->m_buffer.data();
        o[0] = r0_0;
        o[1] = r1_0;
        o[2] = r2_0;
        o[3] = 0.f;
        o[4] = r0_1;
        o[5] = r1_1;
        o[6] = r2_1;
        o[7] = 0.f;
        o[8] = r0_2;
        o[9] = r1_2;
        o[10] = r2_2;
        o[11] = 0.f;
        o[12] = ix;
        o[13] = iy;
        o[14] = iz;
        o[15] = 1.f;
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
