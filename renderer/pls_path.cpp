/*
 * Copyright 2022 Rive
 */

#include "pls_path.hpp"

namespace rive::pls
{
PLSPath::PLSPath(FillRule fillRule, RawPath& rawPath)
{
    m_rawPath.swap(rawPath);
    m_rawPath.pruneEmptySegments();
}

void PLSPath::rewind()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.rewind();
    m_dirt = kAllDirt;
}

void PLSPath::moveTo(float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.moveTo(x, y);
    m_dirt = kAllDirt;
}

void PLSPath::lineTo(float x, float y)
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

void PLSPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
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

void PLSPath::close()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.close();
    m_dirt = kAllDirt;
}

void PLSPath::addRenderPath(RenderPath* path, const Mat2D& matrix)
{
    assert(m_rawPathMutationLockCount == 0);
    PLSPath* plsPath = static_cast<PLSPath*>(path);
    RawPath::Iter transformedPathIter = m_rawPath.addPath(plsPath->m_rawPath, &matrix);
    if (matrix != Mat2D())
    {
        // Prune any segments that became empty after the transform.
        m_rawPath.pruneEmptySegments(transformedPathIter);
    }
    m_dirt = kAllDirt;
}

const AABB& PLSPath::getBounds() const
{
    if (m_dirt & kPathBoundsDirt)
    {
        m_bounds = m_rawPath.bounds();
        m_dirt &= ~kPathBoundsDirt;
    }
    return m_bounds;
}

uint64_t PLSPath::getRawPathMutationID() const
{
    static std::atomic<uint64_t> uniqueIDCounter = 0;
    if (m_dirt & kRawPathMutationIDDirt)
    {
        m_rawPathMutationID = ++uniqueIDCounter;
        m_dirt &= ~kRawPathMutationIDDirt;
    }
    return m_rawPathMutationID;
}
} // namespace rive::pls
