/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_HITTEST_COMMAND_PATH_HPP_
#define _RIVE_HITTEST_COMMAND_PATH_HPP_

#include "rive/command_path.hpp"
#include "rive/math/hit_test.hpp"

namespace rive
{
class HitTester;

class HitTestCommandPath : public CommandPath
{
    HitTester m_Tester;
    Mat2D m_Xform;
    IAABB m_Area;
    FillRule m_FillRule = FillRule::nonZero;

public:
    HitTestCommandPath(const IAABB& area);

    // can call this between calls to move/line/etc.
    void setXform(const Mat2D& xform) { m_Xform = xform; }

    bool wasHit();

    // These 4 are not a good for the hit-tester
    void rewind() override;
    void fillRule(FillRule value) override;
    void addPath(CommandPath* path, const Mat2D& transform) override;
    RenderPath* renderPath() override;

    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    void close() override;
};
} // namespace rive
#endif
