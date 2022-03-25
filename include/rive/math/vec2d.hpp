#ifndef _RIVE_VEC2D_HPP_
#define _RIVE_VEC2D_HPP_

#include "rive/rive_types.hpp"

namespace rive {
    class Mat2D;
    class Vec2D {
    private:
        float m_Buffer[2];

    public:
        constexpr Vec2D() : m_Buffer{0.0f, 0.0f} {}
        constexpr Vec2D(float x, float y) : m_Buffer{x, y} {}
        constexpr Vec2D(const Vec2D&) = default;

        float x() const { return m_Buffer[0]; }
        float y() const { return m_Buffer[1]; }
        inline const float* values() const { return m_Buffer; }

        float& operator[](std::size_t idx) { return m_Buffer[idx]; }
        const float& operator[](std::size_t idx) const { return m_Buffer[idx]; }

        float lengthSquared() const { return x() * x() + y() * y(); }
        float length() const;
        Vec2D normalized() const;

        Vec2D operator-() const { return {-x(), -y()}; }

        void operator*=(float s) {
            m_Buffer[0] *= s;
            m_Buffer[1] *= s;
        }

        friend inline Vec2D operator-(const Vec2D& a, const Vec2D& b) {
            return {a[0] - b[0], a[1] - b[1]};
        }

        static Vec2D lerp(const Vec2D& a, const Vec2D& b, float f);
        static Vec2D transformDir(const Vec2D& a, const Mat2D& m);

        static float dot(Vec2D a, Vec2D b) { return a[0] * b[0] + a[1] * b[1]; }
        static Vec2D scaleAndAdd(Vec2D a, Vec2D b, float scale) {
            return {
                a[0] + b[0] * scale,
                a[1] + b[1] * scale,
            };
        }
        static float distance(const Vec2D& a, const Vec2D& b) { return (a - b).length(); }
        static float distanceSquared(const Vec2D& a, const Vec2D& b) {
            return (a - b).lengthSquared();
        }

        Vec2D& operator+=(Vec2D v) {
            m_Buffer[0] += v[0];
            m_Buffer[1] += v[1];
            return *this;
        }
        Vec2D& operator-=(Vec2D v) {
            m_Buffer[0] -= v[0];
            m_Buffer[1] -= v[1];
            return *this;
        }
    };

    inline Vec2D operator*(const Vec2D& v, float s) { return {v[0] * s, v[1] * s}; }
    inline Vec2D operator*(float s, const Vec2D& v) { return {v[0] * s, v[1] * s}; }
    inline Vec2D operator/(const Vec2D& v, float s) { return {v[0] / s, v[1] / s}; }

    inline Vec2D operator+(const Vec2D& a, const Vec2D& b) { return {a[0] + b[0], a[1] + b[1]}; }

    inline bool operator==(const Vec2D& a, const Vec2D& b) { return a[0] == b[0] && a[1] == b[1]; }
    inline bool operator!=(const Vec2D& a, const Vec2D& b) { return a[0] != b[0] || a[1] != b[1]; }
} // namespace rive
#endif
