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
    m_rawPath.rewind();
    m_boundsDirty = true;
}

void PLSPath::moveTo(float x, float y)
{
    m_rawPath.moveTo(x, y);
    m_boundsDirty = true;
}

void PLSPath::lineTo(float x, float y)
{
    // Make sure to start a new contour, even if this line is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {x, y};
    if (m_rawPath.points().back() != p1)
    {
        m_rawPath.line(p1);
    }

    m_boundsDirty = true;
}

void PLSPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    // Make sure to start a new contour, even if this cubic is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {ox, oy};
    Vec2D p2 = {ix, iy};
    Vec2D p3 = {x, y};
    if (m_rawPath.points().back() != p1 || p1 != p2 || p2 != p3)
    {
        m_rawPath.cubic(p1, p2, p3);
    }

    m_boundsDirty = true;
}

void PLSPath::close()
{
    m_rawPath.close();
    m_boundsDirty = true;
}

void PLSPath::addRenderPath(RenderPath* path, const Mat2D& matrix)
{
    PLSPath* plsPath = static_cast<PLSPath*>(path);
    RawPath::Iter transformedPathIter = m_rawPath.addPath(plsPath->m_rawPath, &matrix);
    if (matrix != Mat2D())
    {
        // Prune any segments that became empty after the transform.
        m_rawPath.pruneEmptySegments(transformedPathIter);
    }
    m_boundsDirty = true;
}

const AABB& PLSPath::getBounds()
{
    if (m_boundsDirty)
    {
        m_bounds = m_rawPath.bounds();
        m_boundsDirty = false;
    }
    return m_bounds;
}
} // namespace rive::pls
