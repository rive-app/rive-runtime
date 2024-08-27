/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/rive_render_factory.hpp"

#include "rive_render_paint.hpp"
#include "rive_render_path.hpp"
#include "rive/renderer/rive_renderer.hpp"

namespace rive::gpu
{
rcp<RenderShader> RiveRenderFactory::makeLinearGradient(float sx,
                                                        float sy,
                                                        float ex,
                                                        float ey,
                                                        const ColorInt colors[], // [count]
                                                        const float stops[],     // [count]
                                                        size_t count)
{

    return PLSGradient::MakeLinear(sx, sy, ex, ey, colors, stops, count);
}

rcp<RenderShader> RiveRenderFactory::makeRadialGradient(float cx,
                                                        float cy,
                                                        float radius,
                                                        const ColorInt colors[], // [count]
                                                        const float stops[],     // [count]
                                                        size_t count)
{

    return PLSGradient::MakeRadial(cx, cy, radius, colors, stops, count);
}

rcp<RenderPath> RiveRenderFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    return make_rcp<RiveRenderPath>(fillRule, rawPath);
}

rcp<RenderPath> RiveRenderFactory::makeEmptyRenderPath() { return make_rcp<RiveRenderPath>(); }

rcp<RenderPaint> RiveRenderFactory::makeRenderPaint() { return make_rcp<RiveRenderPaint>(); }
} // namespace rive::gpu
