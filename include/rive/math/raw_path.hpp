/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RAW_PATH_HPP_
#define _RIVE_RAW_PATH_HPP_

#include "rive/span.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/path_types.hpp"
#include "rive/math/vec2d.hpp"

#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <vector>

namespace rive {

    class RawPath {
    public:
        std::vector<Vec2D> m_Points;
        std::vector<PathVerb> m_Verbs;

        RawPath() {}
        ~RawPath() {}

        bool empty() const { return m_Points.empty(); }
        AABB bounds() const;

        void move(Vec2D);
        void line(Vec2D);
        void quad(Vec2D, Vec2D);
        void cubic(Vec2D, Vec2D, Vec2D);
        void close();

        RawPath transform(const Mat2D&) const;
        void transformInPlace(const Mat2D&);
        RawPath operator*(const Mat2D& mat) const {
            return this->transform(mat);
        }

        Span<const Vec2D> points() const { return toSpan(m_Points); }
        Span<Vec2D> points() { return toSpan(m_Points); }

        Span<const PathVerb> verbs() const { return toSpan(m_Verbs); }
        Span<PathVerb> verbs() { return toSpan(m_Verbs); }

        Span<const uint8_t> verbsU8() const {
            const uint8_t* ptr = (const uint8_t*)m_Verbs.data();
            return Span<const uint8_t>(ptr, m_Verbs.size());
        }

        // Syntactic sugar for x,y -vs- vec2d

        void moveTo(float x, float y) { move({x, y}); }
        void lineTo(float x, float y) { line({x, y}); }
        void quadTo(float x, float y, float x1, float y1) { quad({x, y}, {x1, y1}); }
        void cubicTo(float x, float y, float x1, float y1, float x2, float y2) {
            cubic({x, y}, {x1, y1}, {x2, y2});
        }

        // Helpers for adding new contours

        void addRect(const AABB&, PathDirection = PathDirection::cw);
        void addOval(const AABB&, PathDirection = PathDirection::cw);
        void addPoly(Span<const Vec2D>, bool isClosed);

        class Iter {
            const Vec2D* m_currPts;
            const PathVerb* m_currVerb;
            const PathVerb* m_stopVerb; // 1 past last verb
        public:
            Iter(const RawPath& path) {
                m_currPts  = path.m_Points.data();
                m_currVerb = path.m_Verbs.data();
                m_stopVerb = path.m_Verbs.data() + path.m_Verbs.size();
            }
            
            struct Rec {
                const Vec2D* pts;
                int count;
                PathVerb verb;
                
                operator bool() const { return pts != nullptr; }
            };
            Rec next();
        };
    };

} // namespace rive

#endif
