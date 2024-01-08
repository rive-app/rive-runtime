#ifndef _RIVE_MAT2D_HPP_
#define _RIVE_MAT2D_HPP_

#include "rive/math/aabb.hpp"
#include "rive/math/vec2d.hpp"
#include <array>
#include <cstddef>

namespace rive
{
class TransformComponents;
class Mat2D
{
public:
    constexpr Mat2D() : m_buffer{{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}} {}
    constexpr Mat2D(float x1, float y1, float x2, float y2, float tx, float ty) :
        m_buffer{{x1, y1, x2, y2, tx, ty}}
    {}

    inline const float* values() const { return &m_buffer[0]; }

    float& operator[](std::size_t idx) { return m_buffer[idx]; }
    const float& operator[](std::size_t idx) const { return m_buffer[idx]; }

    static Mat2D fromRotation(float rad);
    static Mat2D fromScale(float sx, float sy) { return {sx, 0, 0, sy, 0, 0}; }
    static Mat2D fromTranslate(float tx, float ty) { return {1, 0, 0, 1, tx, ty}; }
    static Mat2D fromTranslation(Vec2D translation)
    {
        return {1, 0, 0, 1, translation.x, translation.y};
    }
    static Mat2D fromScaleAndTranslation(float sx, float sy, float tx, float ty)
    {
        return {sx, 0, 0, sy, tx, ty};
    }

    void scaleByValues(float sx, float sy);

    Mat2D& operator*=(const Mat2D& rhs)
    {
        *this = Mat2D::multiply(*this, rhs);
        return *this;
    }

    // Sets dst[i] = M * pts[i] for i in 0..n-1.
    void mapPoints(Vec2D dst[], const Vec2D pts[], size_t n) const;

    // Computes a bounding box that would tightly contain the given points if they were to all be
    // transformed by this matrix.
    AABB mapBoundingBox(const Vec2D pts[], size_t n) const;
    AABB mapBoundingBox(const AABB&) const;

    // If returns true, result holds the inverse.
    // If returns false, result is unchnaged.
    bool invert(Mat2D* result) const;

    Mat2D invertOrIdentity() const
    {
        Mat2D inverse;          // initialized to identity
        (void)invert(&inverse); // inverse is unchanged if invert() fails
        return inverse;
    }

    TransformComponents decompose() const;
    static Mat2D compose(const TransformComponents&);
    float findMaxScale() const;
    Mat2D scale(Vec2D) const;

    static Mat2D multiply(const Mat2D& a, const Mat2D& b);

    float xx() const { return m_buffer[0]; }
    float xy() const { return m_buffer[1]; }
    float yx() const { return m_buffer[2]; }
    float yy() const { return m_buffer[3]; }
    float tx() const { return m_buffer[4]; }
    float ty() const { return m_buffer[5]; }

    Vec2D translation() const { return {m_buffer[4], m_buffer[5]}; }

    void xx(float value) { m_buffer[0] = value; }
    void xy(float value) { m_buffer[1] = value; }
    void yx(float value) { m_buffer[2] = value; }
    void yy(float value) { m_buffer[3] = value; }
    void tx(float value) { m_buffer[4] = value; }
    void ty(float value) { m_buffer[5] = value; }

private:
    std::array<float, 6> m_buffer;
};

inline Vec2D operator*(const Mat2D& m, Vec2D v)
{
    return {
        m[0] * v.x + m[2] * v.y + m[4],
        m[1] * v.x + m[3] * v.y + m[5],
    };
}

inline Mat2D operator*(const Mat2D& a, const Mat2D& b) { return Mat2D::multiply(a, b); }

inline bool operator==(const Mat2D& a, const Mat2D& b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && a[4] == b[4] &&
           a[5] == b[5];
}

inline bool operator!=(const Mat2D& a, const Mat2D& b) { return !(a == b); }

} // namespace rive
#endif
