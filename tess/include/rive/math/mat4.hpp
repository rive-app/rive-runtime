#ifndef _RIVE_MAT4_HPP_
#define _RIVE_MAT4_HPP_

#include <cstddef>

namespace rive
{
class Mat2D;
class Mat4
{
private:
    float m_Buffer[16];

public:
    Mat4() :
        m_Buffer{
            // clang-format off
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f, 
                0.0f, 0.0f, 0.0f, 1.0f,
            // clang-format on
        }
    {}
    Mat4(const Mat4& copy) = default;

    // Construct a 4x4 Matrix with the provided elements stored in row-major
    // order.
    Mat4(
        // clang-format off
            float x1, float y1, float z1, float w1,
            float x2, float y2, float z2, float w2,
            float x3, float y3, float z3, float w3,
            float tx, float ty, float tz, float tw
        // clang-format on
        ) :
        m_Buffer{
            // clang-format off
                x1, y1, z1, w1,
                x2, y2, z2, w2,
                x3, y3, z3, w3,
                tx, ty, tz, tw,
            // clang-format on
        }
    {}

    Mat4(const Mat2D& mat2d);

    inline const float* values() const { return m_Buffer; }

    float& operator[](std::size_t idx) { return m_Buffer[idx]; }
    const float& operator[](std::size_t idx) const { return m_Buffer[idx]; }

    Mat4& operator*=(const Mat4& rhs)
    {
        *this = Mat4::multiply(*this, rhs);
        return *this;
    }

    Mat4& operator*=(const Mat2D& rhs)
    {
        *this = Mat4::multiply(*this, rhs);
        return *this;
    }

    static Mat4 multiply(const Mat4& a, const Mat4& b);
    static Mat4 multiply(const Mat4& a, const Mat2D& b);
};
inline Mat4 operator*(const Mat4& a, const Mat4& b) { return Mat4::multiply(a, b); }
inline Mat4 operator*(const Mat4& a, const Mat2D& b) { return Mat4::multiply(a, b); }
} // namespace rive
#endif
