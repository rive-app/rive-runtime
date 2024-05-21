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
#include <tuple>
#include <vector>

namespace rive
{

class CommandPath;

class RawPath
{
public:
    bool operator==(const RawPath& o) const;
    bool operator!=(const RawPath& o) const { return !(*this == o); }

    bool empty() const { return m_Points.empty(); }
    AABB bounds() const;
    size_t countMoveTos() const;

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

    Span<const Vec2D> points() const { return m_Points; }
    Span<Vec2D> points() { return m_Points; }

    Span<const PathVerb> verbs() const { return m_Verbs; }
    Span<PathVerb> verbs() { return m_Verbs; }

    Span<const uint8_t> verbsU8() const
    {
        const uint8_t* ptr = (const uint8_t*)m_Verbs.data();
        return Span<const uint8_t>(ptr, m_Verbs.size());
    }

    // Syntactic sugar for x,y -vs- vec2d

    void moveTo(float x, float y) { move({x, y}); }
    void lineTo(float x, float y) { line({x, y}); }
    void quadTo(float x, float y, float x1, float y1) { quad({x, y}, {x1, y1}); }
    void cubicTo(float x, float y, float x1, float y1, float x2, float y2)
    {
        cubic({x, y}, {x1, y1}, {x2, y2});
    }

    // Helpers for adding new contours

    void addRect(const AABB&, PathDirection = PathDirection::cw);
    void addOval(const AABB&, PathDirection = PathDirection::cw);
    void addPoly(Span<const Vec2D>, bool isClosed);

    // Simple STL-style iterator. To traverse using range-for:
    //
    //   for (auto [verb, pts] : rawPath) { ... }
    //
    class Iter
    {
    public:
        Iter() = default;
        Iter(const PathVerb* verbs, const Vec2D* pts) : m_verbs(verbs), m_pts(pts) {}

        bool operator!=(const Iter& that) const
        {
            assert(m_verbs != that.m_verbs || m_pts == that.m_pts);
            return m_verbs != that.m_verbs;
        }
        bool operator==(const Iter& that) const
        {
            assert(m_verbs != that.m_verbs || m_pts == that.m_pts);
            return m_verbs == that.m_verbs;
        }

        // Generic accessors. The points pointer is adjusted to point to p0 for each specific verb.
        PathVerb verb() const { return *m_verbs; }
        const Vec2D* pts() const { return m_pts + PtsBacksetForVerb(verb()); }
        std::tuple<PathVerb, const Vec2D*> operator*() const
        {
            PathVerb verb = *m_verbs;
            return {verb, m_pts + PtsBacksetForVerb(verb)};
        }

        // Specific point accessors for callers who already know the verb. (These may be a tiny bit
        // faster in some cases since the iterator doesn't have to check the verb.)
        Vec2D movePt() const
        {
            assert(verb() == PathVerb::move);
            return m_pts[0];
        }
        const Vec2D* linePts() const
        {
            assert(verb() == PathVerb::line);
            return m_pts - 1;
        }
        const Vec2D* quadPts() const
        {
            assert(verb() == PathVerb::quad);
            return m_pts - 1;
        }
        const Vec2D* cubicPts() const
        {
            assert(verb() == PathVerb::cubic);
            return m_pts - 1;
        }
        Vec2D ptBeforeClose() const
        {
            assert(verb() == PathVerb::close);
            return m_pts[-1];
        }
        // P0 for a close can be accessed via rawPtsPtr()[-1]. Note than p1 for a close is not in
        // the array at this location.

        // Internal pointers held by the iterator. See PtsBacksetForVerb() for how pts() relates to
        // the data for specific verbs.
        const PathVerb* rawVerbsPtr() const { return m_verbs; }
        const Vec2D* rawPtsPtr() const { return m_pts; }

        Iter& operator++() // "++iter"
        {
            m_pts += PtsAdvanceAfterVerb(*m_verbs++);
            return *this;
        }

        // How much should we advance pts after encountering this verb?
        inline static int PtsAdvanceAfterVerb(PathVerb verb)
        {
            switch (verb)
            {
                case PathVerb::move:
                    return 1;
                case PathVerb::line:
                    return 1;
                case PathVerb::quad:
                    return 2;
                case PathVerb::cubic:
                    return 3;
                case PathVerb::close:
                    return 0;
            }
            RIVE_UNREACHABLE();
        }

        // Where is p0 relative to our m_pts pointer? We find the start point of segments by
        // peeking backwards from the current point, which works as long as there is always a
        // PathVerb::move before any geometry. (injectImplicitMoveToIfNeeded() guarantees this
        // to be the case.)
        inline static int PtsBacksetForVerb(PathVerb verb)
        {
            switch (verb)
            {
                case PathVerb::move:
                    return 0;
                case PathVerb::line:
                    return -1;
                case PathVerb::quad:
                    return -1;
                case PathVerb::cubic:
                    return -1;
                case PathVerb::close:
                    return -1;
            }
            RIVE_UNREACHABLE();
        }

    private:
        const PathVerb* m_verbs;
        const Vec2D* m_pts;
    };
    Iter begin() const { return {m_Verbs.data(), m_Points.data()}; }
    Iter end() const
    {
        return {m_Verbs.data() + m_Verbs.size(), m_Points.data() + m_Points.size()};
    }

    template <typename Handler> RawPath morph(Handler proc) const
    {
        RawPath dst;
        // todo: dst.reserve(src.ptCount, src.verbCount);
        for (auto iter : *this)
        {
            PathVerb verb = std::get<0>(iter);
            const Vec2D* pts = std::get<1>(iter);
            switch (verb)
            {
                case PathVerb::move:
                    dst.move(proc(pts[0]));
                    break;
                case PathVerb::line:
                    dst.line(proc(pts[1]));
                    break;
                case PathVerb::quad:
                    dst.quad(proc(pts[1]), proc(pts[2]));
                    break;
                case PathVerb::cubic:
                    dst.cubic(proc(pts[1]), proc(pts[2]), proc(pts[3]));
                    break;
                case PathVerb::close:
                    dst.close();
                    break;
            }
        }
        return dst;
    }

    // Adds the given RawPath to the end of this path, with an optional transform.
    // Returns an iterator at the beginning of the newly added geometry.
    Iter addPath(const RawPath&, const Mat2D* = nullptr);

    void pruneEmptySegments(Iter start);
    void pruneEmptySegments() { pruneEmptySegments(begin()); }

    // Utility for pouring a RawPath into a CommandPath
    void addTo(CommandPath*) const;

    // If there is not currently an open contour, this method opens a new contour at the current pen
    // location, or [0,0] if the path is empty. Otherwise it does nothing.
    void injectImplicitMoveIfNeeded();

private:
    std::vector<Vec2D> m_Points;
    std::vector<PathVerb> m_Verbs;
    size_t m_lastMoveIdx;
    // True of the path is nonempty and the most recent verb is not "close".
    bool m_contourIsOpen = false;
};

} // namespace rive

#endif
