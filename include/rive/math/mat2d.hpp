#ifndef _RIVE_MAT2D_HPP_
#define _RIVE_MAT2D_HPP_

#include "rive/math/vec2d.hpp"
#include <cstddef>

namespace rive {
    class TransformComponents;
    class Mat2D {
    private:
        float m_Buffer[6];

    public:
        Mat2D() : m_Buffer{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f} {}
        Mat2D(const Mat2D& copy) = default;
        Mat2D(float x1, float y1, float x2, float y2, float tx, float ty) :
            m_Buffer{x1, y1, x2, y2, tx, ty} {}

        inline const float* values() const { return m_Buffer; }

        float& operator[](std::size_t idx) { return m_Buffer[idx]; }
        const float& operator[](std::size_t idx) const { return m_Buffer[idx]; }

        static Mat2D fromRotation(float rad);
        static Mat2D fromScale(float sx, float sy) { return {sx, 0, 0, sy, 0, 0}; }
        static Mat2D fromTranslate(float tx, float ty) { return {1, 0, 0, 1, tx, ty}; }

        void scaleByValues(float sx, float sy);

        Mat2D& operator*=(const Mat2D& rhs) {
            *this = Mat2D::multiply(*this, rhs);
            return *this;
        }

        static Mat2D scale(const Mat2D& mat, const Vec2D& vec);
        static Mat2D multiply(const Mat2D& a, const Mat2D& b);
        static bool invert(Mat2D& result, const Mat2D& a);
        static void decompose(TransformComponents& result, const Mat2D& m);
        static void compose(Mat2D& result, const TransformComponents& components);

        float xx() const { return m_Buffer[0]; }
        float xy() const { return m_Buffer[1]; }
        float yx() const { return m_Buffer[2]; }
        float yy() const { return m_Buffer[3]; }
        float tx() const { return m_Buffer[4]; }
        float ty() const { return m_Buffer[5]; }

        void xx(float value) { m_Buffer[0] = value; }
        void xy(float value) { m_Buffer[1] = value; }
        void yx(float value) { m_Buffer[2] = value; }
        void yy(float value) { m_Buffer[3] = value; }
        void tx(float value) { m_Buffer[4] = value; }
        void ty(float value) { m_Buffer[5] = value; }
    };

    inline Vec2D operator*(const Mat2D& m, Vec2D v) {
        return {
            m[0] * v.x() + m[2] * v.y() + m[4],
            m[1] * v.x() + m[3] * v.y() + m[5],
        };
    }

    inline Mat2D operator*(const Mat2D& a, const Mat2D& b) { return Mat2D::multiply(a, b); }

    inline bool operator==(const Mat2D& a, const Mat2D& b) {
        return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && a[4] == b[4] &&
               a[5] == b[5];
    }
} // namespace rive
#endif
