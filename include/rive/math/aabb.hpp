#ifndef _RIVE_AABB_HPP_
#define _RIVE_AABB_HPP_

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include <cstddef>

namespace rive {
    class AABB {
    public:
        union {
            float buffer[4];
            struct {
                float minX, minY, maxX, maxY;
            };
        };

    public:
        AABB() : buffer{0} {}
        AABB(const AABB& o) :
            minX(o.minX), minY(o.minY), maxX(o.maxX), maxY(o.maxY)
        {}
        
        AABB(float minX, float minY, float maxX, float maxY) :
            minX(minX), minY(minY), maxX(maxX), maxY(maxY)
        {}

        bool operator==(const AABB& o) const {
            return minX == o.minX && minY == o.minY &&
                   maxX == o.maxX && maxY == o.maxY;
        }
        bool operator!=(const AABB& o) const {
            return !(*this == o);
        }

        inline const float* values() const { return buffer; }

        float& operator[](std::size_t idx) { return buffer[idx]; }
        const float& operator[](std::size_t idx) const { return buffer[idx]; }

        static bool contains(const AABB& a, const AABB& b);
        static bool isValid(const AABB& a);
        static bool testOverlap(const AABB& a, const AABB& b);
        static bool areIdentical(const AABB& a, const AABB& b);
        static void transform(AABB& out, const AABB& a, const Mat2D& matrix);

        float left() const { return minX; }
        float top() const { return minY; }
        float right() const { return maxX; }
        float bottom() const { return maxY; }

        float width() const { return maxX - minX; }
        float height() const { return maxY - minY; }
        Vec2D size() const {
            return {width(), height()};
        }
        Vec2D center() const {
            return {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
        }

        AABB inset(float dx, float dy) const {
            AABB r = {minX + dx, minY + dy, maxX - dx, maxY - dy};
            assert(r.width() >= 0);
            assert(r.height() >= 0);
            return r;
        }
        AABB offset(float dx, float dy) const {
            return {minX + dx, minY + dy, maxX + dx, maxY + dy};
        }
    };
} // namespace rive
#endif
