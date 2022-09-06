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

class CommandPath;

class RawPath {
public:
    bool operator==(const RawPath& o) const;
    bool operator!=(const RawPath& o) const { return !(*this == o); }

    bool empty() const { return m_Points.empty(); }
    AABB bounds() const;

    void move(Vec2D);
    void line(Vec2D);
    void quad(Vec2D, Vec2D);
    void cubic(Vec2D, Vec2D, Vec2D);
    void close();

    void swap(RawPath&);

    // Makes the path empty and frees any memory allocated by the drawing
    // (line, curve, move, close) calls.
    void reset();

    // Makes the path empty but keeps the memory for the drawing calls reserved.
    void rewind();

    RawPath transform(const Mat2D&) const;
    void transformInPlace(const Mat2D&);
    RawPath operator*(const Mat2D& mat) const { return this->transform(mat); }

    Span<const Vec2D> points() const { return m_Points; }
    Span<Vec2D> points() { return m_Points; }

    Span<const PathVerb> verbs() const { return m_Verbs; }
    Span<PathVerb> verbs() { return m_Verbs; }

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

    void addPath(const RawPath&, const Mat2D* = nullptr);

    // Simple STL-style iterator. To traverse using range-for:
    //
    //   for (auto [verb, pts] : rawPath) { ... }
    //
    class Iter {
    public:
        Iter() = default;
        Iter(const PathVerb* verbs, const Vec2D* pts) : m_verbs(verbs), m_pts(pts) {}

        bool operator!=(const Iter& that) const { return m_verbs != that.m_verbs; }
        bool operator==(const Iter& that) const { return m_verbs == that.m_verbs; }

        PathVerb peekVerb() const { return *m_verbs; }

        std::tuple<const PathVerb, const Vec2D* const> operator*() const {
            PathVerb verb = peekVerb();
            return {verb, m_pts + PtsBacksetForVerb(verb)};
        }

        Iter& operator++() { // ++iter
            m_pts += PtsAdvanceAfterVerb(*m_verbs++);
            return *this;
        }

    private:
        // How much should we advance pts after encountering this verb?
        constexpr static int PtsAdvanceAfterVerb(PathVerb verb) {
            switch (verb) {
                case PathVerb::move: return 1;
                case PathVerb::line: return 1;
                case PathVerb::quad: return 2;
                case PathVerb::cubic: return 3;
                case PathVerb::close: return 0;
            }
            RIVE_UNREACHABLE;
        }

        // Where is p0 relative to our m_pts pointer? We find the start point of segments by
        // peeking backwards from the current point, which works as long as there is always a
        // PathVerb::move before any geometry. (injectImplicitMoveToIfNeeded() guarantees this
        // to be the case.)
        constexpr static int PtsBacksetForVerb(PathVerb verb) {
            switch (verb) {
                case PathVerb::move: return 0;
                case PathVerb::line: return -1;
                case PathVerb::quad: return -1;
                case PathVerb::cubic: return -1;
                case PathVerb::close: return -1;
            }
            RIVE_UNREACHABLE;
        }

        const PathVerb* m_verbs;
        const Vec2D* m_pts;
    };
    Iter begin() const { return {m_Verbs.data(), m_Points.data()}; }
    Iter end() const { return {m_Verbs.data() + m_Verbs.size(), nullptr}; }

    template <typename Handler> RawPath morph(Handler proc) const {
        RawPath dst;
        // todo: dst.reserve(src.ptCount, src.verbCount);
        for (auto [verb, pts] : *this) {
            switch (verb) {
                case PathVerb::move: dst.move(proc(pts[0])); break;
                case PathVerb::line: dst.line(proc(pts[1])); break;
                case PathVerb::quad: dst.quad(proc(pts[1]), proc(pts[2])); break;
                case PathVerb::cubic: dst.cubic(proc(pts[1]), proc(pts[2]), proc(pts[3])); break;
                case PathVerb::close: dst.close(); break;
            }
        }
        return dst;
    }

    // Utility for pouring a RawPath into a CommandPath
    void addTo(CommandPath*) const;

private:
    void injectImplicitMoveIfNeeded();

    std::vector<Vec2D> m_Points;
    std::vector<PathVerb> m_Verbs;
    size_t m_lastMoveIdx;
    // True of the path is nonempty and the most recent verb is not "close".
    bool m_contourIsOpen = false;
};

} // namespace rive

#endif
