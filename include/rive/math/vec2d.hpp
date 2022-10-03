#ifndef _RIVE_VEC2D_HPP_
#define _RIVE_VEC2D_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
class Mat2D;
class Vec2D
{
public:
    float x, y;

    constexpr Vec2D() : x(0), y(0) {}
    constexpr Vec2D(float x, float y) : x(x), y(y) {}
    constexpr Vec2D(const Vec2D&) = default;

    float lengthSquared() const { return x * x + y * y; }
    float length() const;
    Vec2D normalized() const;

    // Normalize this Vec, and return its previous length
    float normalizeLength()
    {
        const float len = this->length();
        x /= len;
        y /= len;
        return len;
    }

    Vec2D operator-() const { return {-x, -y}; }

    void operator*=(float s)
    {
        x *= s;
        y *= s;
    }

    void operator/=(float s)
    {
        x /= s;
        y /= s;
    }

    friend inline Vec2D operator-(const Vec2D& a, const Vec2D& b) { return {a.x - b.x, a.y - b.y}; }

    static inline Vec2D lerp(Vec2D a, Vec2D b, float f);

    static Vec2D transformDir(const Vec2D& a, const Mat2D& m);

    static float dot(Vec2D a, Vec2D b) { return a.x * b.x + a.y * b.y; }
    static float cross(Vec2D a, Vec2D b) { return a.x * b.y - a.y * b.x; }
    static Vec2D scaleAndAdd(Vec2D a, Vec2D b, float scale)
    {
        return {
            a.x + b.x * scale,
            a.y + b.y * scale,
        };
    }
    static float distance(const Vec2D& a, const Vec2D& b) { return (a - b).length(); }
    static float distanceSquared(const Vec2D& a, const Vec2D& b) { return (a - b).lengthSquared(); }

    Vec2D& operator+=(Vec2D v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2D& operator-=(Vec2D v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
};

inline Vec2D operator*(const Vec2D& v, float s) { return {v.x * s, v.y * s}; }
inline Vec2D operator*(float s, const Vec2D& v) { return {v.x * s, v.y * s}; }
inline Vec2D operator/(const Vec2D& v, float s) { return {v.x / s, v.y / s}; }

inline Vec2D operator+(const Vec2D& a, const Vec2D& b) { return {a.x + b.x, a.y + b.y}; }

inline bool operator==(const Vec2D& a, const Vec2D& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Vec2D& a, const Vec2D& b) { return a.x != b.x || a.y != b.y; }

Vec2D Vec2D::lerp(Vec2D a, Vec2D b, float t) { return a + (b - a) * t; }

} // namespace rive
#endif
