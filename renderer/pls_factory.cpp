/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_factory.hpp"

#include "pls_paint.hpp"
#include "pls_path.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
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

std::unique_ptr<RenderPath> PLSFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    return std::make_unique<PLSPath>(fillRule, rawPath);
}

std::unique_ptr<RenderPath> PLSFactory::makeEmptyRenderPath()
{
    return std::make_unique<PLSPath>();
}

std::unique_ptr<RenderPaint> PLSFactory::makeRenderPaint() { return std::make_unique<PLSPaint>(); }
} // namespace rive::pls
