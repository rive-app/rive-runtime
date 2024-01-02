/*
 * Copyright 2022 Rive
 */

#include "rive/math/raw_path.hpp"

#include "rive/command_path.hpp"
#include "rive/math/simd.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace rive
{
bool RawPath::operator==(const RawPath& o) const
{
    return m_Points == o.m_Points && m_Verbs == o.m_Verbs;
}

AABB RawPath::bounds() const
{
    float4 mins, maxes;
    size_t i;
    if (m_Points.size() & 1)
    {
        mins = maxes = simd::load2f(&m_Points[0].x).xyxy;
        i = 1;
    }
    else
    {
        mins = maxes = m_Points.empty() ? float4{0, 0, 0, 0} : simd::load4f(&m_Points[0].x);
        i = 2;
    }
    for (; i < m_Points.size(); i += 2)
    {
        float4 pts = simd::load4f(&m_Points[i].x);
        mins = simd::min(mins, pts);
        maxes = simd::max(maxes, pts);
    }
    AABB bounds;
    simd::store(&bounds.minX, simd::min(mins.xy, mins.zw));
    simd::store(&bounds.maxX, simd::max(maxes.xy, maxes.zw));
    return bounds;
}

size_t RawPath::countMoveTos() const
{
    size_t moveToCount = 0;
    for (PathVerb verb : m_Verbs)
    {
        moveToCount += verb == PathVerb::move ? 1 : 0;
    }
    return moveToCount;
}

void RawPath::injectImplicitMoveIfNeeded()
{
    if (!m_contourIsOpen)
    {
        move(m_Points.empty() ? Vec2D{0, 0} : m_Points[m_lastMoveIdx]);
    }
}

void RawPath::move(Vec2D a)
{
    m_contourIsOpen = true;
    m_lastMoveIdx = m_Points.size();
    m_Points.push_back(a);
    m_Verbs.push_back(PathVerb::move);
}

void RawPath::line(Vec2D a)
{
    injectImplicitMoveIfNeeded();
    m_Points.push_back(a);
    m_Verbs.push_back(PathVerb::line);
}

void RawPath::quad(Vec2D a, Vec2D b)
{
    injectImplicitMoveIfNeeded();
    m_Points.push_back(a);
    m_Points.push_back(b);
    m_Verbs.push_back(PathVerb::quad);
}

void RawPath::cubic(Vec2D a, Vec2D b, Vec2D c)
{
    injectImplicitMoveIfNeeded();
    m_Points.push_back(a);
    m_Points.push_back(b);
    m_Points.push_back(c);
    m_Verbs.push_back(PathVerb::cubic);
}

void RawPath::close()
{
    if (m_contourIsOpen)
    {
        m_Verbs.push_back(PathVerb::close);
        m_contourIsOpen = false;
    }
}

RawPath RawPath::transform(const Mat2D& m) const
{
    RawPath path;

    path.m_Verbs = m_Verbs;

    path.m_Points.resize(m_Points.size());
    m.mapPoints(path.m_Points.data(), m_Points.data(), m_Points.size());
    return path;
}

void RawPath::transformInPlace(const Mat2D& m)
{
    m.mapPoints(m_Points.data(), m_Points.data(), m_Points.size());
}

void RawPath::addRect(const AABB& r, PathDirection dir)
{
    // We manually close the rectangle, in case we want to stroke
    // this path. We also call close() so we get proper joins
    // (and not caps).

    m_Points.reserve(5);
    m_Verbs.reserve(6);

    moveTo(r.left(), r.top());
    if (dir == PathDirection::clockwise)
    {
        lineTo(r.right(), r.top());
        lineTo(r.right(), r.bottom());
        lineTo(r.left(), r.bottom());
    }
    else
    {
        lineTo(r.left(), r.bottom());
        lineTo(r.right(), r.bottom());
        lineTo(r.right(), r.top());
    }
    close();
}

void RawPath::addOval(const AABB& r, PathDirection dir)
{
    // see https://spencermortensen.com/articles/bezier-circle/
    constexpr float C = 0.5519150244935105707435627f;
    // precompute clockwise unit circle, starting and ending at {1, 0}
    constexpr rive::Vec2D unit[] = {
        {1, 0},
        {1, C},
        {C, 1}, // quadrant 1 ( 4:30)
        {0, 1},
        {-C, 1},
        {-1, C}, // quadrant 2 ( 7:30)
        {-1, 0},
        {-1, -C},
        {-C, -1}, // quadrant 3 (10:30)
        {0, -1},
        {C, -1},
        {1, -C}, // quadrant 4 ( 1:30)
        {1, 0},
    };

    const auto center = r.center();
    const float dx = center.x;
    const float dy = center.y;
    const float sx = r.width() * 0.5f;
    const float sy = r.height() * 0.5f;

    auto map = [dx, dy, sx, sy](rive::Vec2D p) {
        return rive::Vec2D(p.x * sx + dx, p.y * sy + dy);
    };

    m_Points.reserve(13);
    m_Verbs.reserve(6);

    if (dir == PathDirection::clockwise)
    {
        move(map(unit[0]));
        for (int i = 1; i <= 12; i += 3)
        {
            cubic(map(unit[i + 0]), map(unit[i + 1]), map(unit[i + 2]));
        }
    }
    else
    {
        move(map(unit[12]));
        for (int i = 11; i >= 0; i -= 3)
        {
            cubic(map(unit[i - 0]), map(unit[i - 1]), map(unit[i - 2]));
        }
    }
    close();
}

void RawPath::addPoly(Span<const Vec2D> span, bool isClosed)
{
    if (span.size() == 0)
    {
        return;
    }

    // should we permit must moveTo() or just moveTo()/close() ?

    m_Points.reserve(span.size() + isClosed);
    m_Verbs.reserve(span.size() + isClosed);

    move(span[0]);
    for (size_t i = 1; i < span.size(); ++i)
    {
        line(span[i]);
    }
    if (isClosed)
    {
        close();
    }
}

RawPath::Iter RawPath::addPath(const RawPath& src, const Mat2D* mat)
{
    size_t initialVerbCount = m_Verbs.size();
    size_t initialPointCount = m_Points.size();

    m_Verbs.insert(m_Verbs.end(), src.m_Verbs.cbegin(), src.m_Verbs.cend());

    if (mat)
    {
        const auto oldPointCount = m_Points.size();
        m_Points.resize(oldPointCount + src.m_Points.size());
        Vec2D* dst = m_Points.data() + oldPointCount;
        mat->mapPoints(dst, src.m_Points.data(), src.m_Points.size());
    }
    else
    {
        m_Points.insert(m_Points.end(), src.m_Points.cbegin(), src.m_Points.cend());
    }

    // Return an iterator at the beginning of the newly added geometry.
    return Iter{m_Verbs.data() + initialVerbCount, m_Points.data() + initialPointCount};
}

void RawPath::pruneEmptySegments(Iter start)
{
    auto dstVerb =
        m_Verbs.begin() + std::distance<const PathVerb*>(m_Verbs.data(), start.rawVerbsPtr());
    auto dstPts =
        m_Points.begin() + std::distance<const Vec2D*>(m_Points.data(), start.rawPtsPtr());
    decltype(m_Verbs)::const_iterator srcVerb = dstVerb;
    decltype(m_Points)::const_iterator srcPts = dstPts;

    int ptsAdvance;
    for (auto end = m_Verbs.end(); srcVerb != end; ++srcVerb, srcPts += ptsAdvance)
    {
        PathVerb verb = *srcVerb;
        ptsAdvance = Iter::PtsAdvanceAfterVerb(verb);

        switch (verb)
        {
            case PathVerb::move:
                break;
            case PathVerb::close:
                break;
            case PathVerb::cubic:
                if (srcPts[2] != srcPts[1])
                {
                    break;
                }
                RIVE_FALLTHROUGH;
            case PathVerb::quad:
                if (srcPts[1] != srcPts[0])
                {
                    break;
                }
                RIVE_FALLTHROUGH;
            case PathVerb::line:
                if (srcPts[0] != srcPts[-1])
                {
                    break;
                }
                // This segment is empty! Don't keep it.
                continue;
        }

        if (srcVerb != dstVerb)
        {
            *dstVerb = verb;
            std::copy(srcPts, srcPts + ptsAdvance, dstPts);
        }

        ++dstVerb;
        dstPts += ptsAdvance;
    }

    if (dstVerb != srcVerb)
    {
        m_Verbs.erase(dstVerb, m_Verbs.end());
        m_Points.erase(dstPts, m_Points.end());
    }
}

//////////////////////////////////////////////////////////////////////////
int path_verb_to_point_count(PathVerb v)
{
    static uint8_t ptCounts[] = {
        1, // move
        1, // line
        2, // quad
        2, // conic (unused)
        3, // cubic
        0, // close
    };
    size_t index = (size_t)v;
    assert(index < sizeof(ptCounts));
    return ptCounts[index];
}

void RawPath::swap(RawPath& rawPath)
{
    m_Points.swap(rawPath.m_Points);
    m_Verbs.swap(rawPath.m_Verbs);
}

void RawPath::reset()
{
    m_Points.clear();
    m_Points.shrink_to_fit();
    m_Verbs.clear();
    m_Verbs.shrink_to_fit();
    m_contourIsOpen = false;
}

void RawPath::rewind()
{
    m_Points.clear();
    m_Verbs.clear();
    m_contourIsOpen = false;
}

///////////////////////////////////

void RawPath::addTo(CommandPath* result) const
{
    for (auto iter : *this)
    {
        PathVerb verb = std::get<0>(iter);
        const Vec2D* pts = std::get<1>(iter);
        switch (verb)
        {
            case PathVerb::move:
                result->move(pts[0]);
                break;
            case PathVerb::line:
                result->line(pts[1]);
                break;
            case PathVerb::cubic:
                result->cubic(pts[1], pts[2], pts[3]);
                break;
            case PathVerb::close:
                result->close();
                break;
            case PathVerb::quad:
                // TODO: actually supports quads.
                result->cubic(Vec2D::lerp(pts[0], pts[1], 2 / 3.f),
                              Vec2D::lerp(pts[2], pts[1], 2 / 3.f),
                              pts[2]);
        }
    }
}
} // namespace rive
