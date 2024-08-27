/*
 * Copyright 2022 Rive
 */

#include "rive_render_path.hpp"

#include "eval_cubic.hpp"
#include "rive/math/simd.hpp"
#include "rive/math/wangs_formula.hpp"

namespace rive::gpu
{
RiveRenderPath::RiveRenderPath(FillRule fillRule, RawPath& rawPath)
{
    m_rawPath.swap(rawPath);
    m_rawPath.pruneEmptySegments();
}

void RiveRenderPath::rewind()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.rewind();
    m_dirt = kAllDirt;
}

void RiveRenderPath::moveTo(float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.moveTo(x, y);
    m_dirt = kAllDirt;
}

void RiveRenderPath::lineTo(float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);

    // Make sure to start a new contour, even if this line is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {x, y};
    if (m_rawPath.points().back() != p1)
    {
        m_rawPath.line(p1);
    }

    m_dirt = kAllDirt;
}

void RiveRenderPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);

    // Make sure to start a new contour, even if this cubic is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {ox, oy};
    Vec2D p2 = {ix, iy};
    Vec2D p3 = {x, y};
    if (m_rawPath.points().back() != p1 || p1 != p2 || p2 != p3)
    {
        m_rawPath.cubic(p1, p2, p3);
    }

    m_dirt = kAllDirt;
}

void RiveRenderPath::close()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.close();
    m_dirt = kAllDirt;
}

void RiveRenderPath::addRenderPath(RenderPath* path, const Mat2D& matrix)
{
    assert(m_rawPathMutationLockCount == 0);
    RiveRenderPath* plsPath = static_cast<RiveRenderPath*>(path);
    RawPath::Iter transformedPathIter = m_rawPath.addPath(plsPath->m_rawPath, &matrix);
    if (matrix != Mat2D())
    {
        // Prune any segments that became empty after the transform.
        m_rawPath.pruneEmptySegments(transformedPathIter);
    }
    m_dirt = kAllDirt;
}

const AABB& RiveRenderPath::getBounds() const
{
    if (m_dirt & kPathBoundsDirt)
    {
        m_bounds = m_rawPath.bounds();
        m_dirt &= ~kPathBoundsDirt;
    }
    return m_bounds;
}

float RiveRenderPath::getCoarseArea() const
{
    if (m_dirt & kPathCoarseAreaDirt)
    {
        float a = 0;
        Vec2D contourP0 = {0, 0}, lastPt = {0, 0};
        for (auto [verb, pts] : m_rawPath)
        {
            switch (verb)
            {
                case PathVerb::move:
                    a += Vec2D::cross(lastPt, contourP0);
                    contourP0 = lastPt = pts[0];
                    break;
                case PathVerb::close:
                    break;
                case PathVerb::line:
                    a += Vec2D::cross(lastPt, pts[1]);
                    lastPt = pts[1];
                    break;
                case PathVerb::quad:
                    RIVE_UNREACHABLE();
                case PathVerb::cubic:
                {
                    // Linearize the cubic in artboard space, then add up the area for each segment.
                    float n = ceilf(wangs_formula::cubic(pts, 1.f / kCoarseAreaTolerance));
                    if (n > 1)
                    {
                        n = std::min(n, 64.f);
                        float4 t = float4{1, 1, 2, 2} * (1 / n);
                        float4 dt = t.w;
                        EvalCubic evalCubic(pts);
                        for (; t.x < 1; t += dt)
                        {
                            float4 p = evalCubic.at(t);
                            Vec2D lo = {p.x, p.y};
                            a += Vec2D::cross(lastPt, lo);
                            lastPt = lo;
                            if (t.y < 1)
                            {
                                Vec2D hi = {p.z, p.w};
                                a += Vec2D::cross(lastPt, hi);
                                lastPt = hi;
                            }
                        }
                    }
                    a += Vec2D::cross(lastPt, pts[3]);
                    lastPt = pts[3];
                    break;
                }
            }
        }
        a += Vec2D::cross(lastPt, contourP0);
        m_coarseArea = a * .5f;
        m_dirt &= ~kPathCoarseAreaDirt;
    }
    return m_coarseArea;
}

uint64_t RiveRenderPath::getRawPathMutationID() const
{
    static std::atomic<uint64_t> uniqueIDCounter = 0;
    if (m_dirt & kRawPathMutationIDDirt)
    {
        m_rawPathMutationID = ++uniqueIDCounter;
        m_dirt &= ~kRawPathMutationIDDirt;
    }
    return m_rawPathMutationID;
}
} // namespace rive::gpu
