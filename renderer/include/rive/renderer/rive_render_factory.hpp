/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/factory.hpp"

namespace rive
{
class RiveRenderPaint;
class RiveRenderPath;

// Partial rive::Factory implementation for the PLS objects that are
// backend-agnostic.
class RiveRenderFactory : public Factory
{
public:
    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    rcp<RenderPath> makeEmptyRenderPath() override;

    rcp<RenderPaint> makeRenderPaint() override;
};

// Narrow an object back to the concrete type a RiveRenderFactory produced
// (asserts on incorrect type).
RiveRenderPath* asRiveRenderPath(RenderPath*);
const RiveRenderPath* asRiveRenderPath(const RenderPath*);
RiveRenderPaint* asRiveRenderPaint(RenderPaint*);
const RiveRenderPaint* asRiveRenderPaint(const RenderPaint*);
} // namespace rive
