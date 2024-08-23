/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_factory.hpp"

#include "pls_paint.hpp"
#include "pls_path.hpp"
#include "rive/pls/pls_renderer.hpp"

namespace rive::pls
{
rcp<RenderShader> PLSFactory::makeLinearGradient(float sx,
                                                 float sy,
                                                 float ex,
                                                 float ey,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count)
{

    return PLSGradient::MakeLinear(sx, sy, ex, ey, colors, stops, count);
}

rcp<RenderShader> PLSFactory::makeRadialGradient(float cx,
                                                 float cy,
                                                 float radius,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count)
{

    return PLSGradient::MakeRadial(cx, cy, radius, colors, stops, count);
}

rcp<RenderPath> PLSFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    return make_rcp<PLSPath>(fillRule, rawPath);
}

rcp<RenderPath> PLSFactory::makeEmptyRenderPath() { return make_rcp<PLSPath>(); }

rcp<RenderPaint> PLSFactory::makeRenderPaint() { return make_rcp<PLSPaint>(); }
} // namespace rive::pls
