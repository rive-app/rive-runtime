/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"

namespace rive::pls
{
// RenderPath implementation for Rive's pixel local storage renderer.
class PLSPath : public RenderPath
{
public:
    PLSPath() = default;
    PLSPath(FillRule fillRule, RawPath& rawPath) { m_rawPath.swap(rawPath); }

    void rewind() override
    {
        m_rawPath.rewind();
        m_boundsDirty = true;
    }
    void fillRule(FillRule rule) override { m_fillRule = rule; }

    void moveTo(float x, float y) override
    {
        m_rawPath.moveTo(x, y);
        m_boundsDirty = true;
    }
    void lineTo(float x, float y) override
    {
        m_rawPath.lineTo(x, y);
        m_boundsDirty = true;
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        m_rawPath.cubicTo(ox, oy, ix, iy, x, y);
        m_boundsDirty = true;
    }
    void close() override
    {
        m_rawPath.close();
        m_boundsDirty = true;
    }

    void addPath(CommandPath* path, const Mat2D& matrix) override
    {
        addRenderPath(path->renderPath(), matrix);
        m_boundsDirty = true;
    }
    void addRenderPath(RenderPath* path, const Mat2D& matrix) override
    {
        m_rawPath.addPath(static_cast<PLSPath*>(path)->m_rawPath, &matrix);
        m_boundsDirty = true;
    }

    const RawPath& getRawPath() const { return m_rawPath; }
    FillRule getFillRule() const { return m_fillRule; }

    const AABB& getBounds()
    {
        if (m_boundsDirty)
        {
            m_bounds = m_rawPath.bounds();
            m_boundsDirty = false;
        }
        return m_bounds;
    }

private:
    FillRule m_fillRule = FillRule::nonZero;
    RawPath m_rawPath;
    AABB m_bounds;
    bool m_boundsDirty = true;
};
} // namespace rive::pls
