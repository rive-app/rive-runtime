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
rcp<RenderBuffer> PLSFactory::makeBufferU16(Span<const uint16_t> data) { return nullptr; }

rcp<RenderBuffer> PLSFactory::makeBufferU32(Span<const uint32_t> data) { return nullptr; }

rcp<RenderBuffer> PLSFactory::makeBufferF32(Span<const float> data) { return nullptr; }

rcp<RenderShader> PLSFactory::makeLinearGradient(float sx,
                                                 float sy,
                                                 float ex,
                                                 float ey,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count)
{
    float2 start = {sx, sy};
    float2 end = {ex, ey};
    PLSGradDataArray<ColorInt> newColors(colors, count);
    PLSGradDataArray<float> newStops(stops, count);

    // If the stops don't begin and end on 0 and 1, transform the gradient so they do. This allows
    // us to take full advantage of the gradient's range of pixels in the texture.
    float firstStop = stops[0];
    float lastStop = stops[count - 1];
    if (firstStop != 0 || lastStop != 1)
    {
        // Tighten the endpoints to align with the mininum and maximum gradient stops.
        float4 newEndpoints = simd::precise_mix(start.xyxy,
                                                end.xyxy,
                                                float4{firstStop, firstStop, lastStop, lastStop});
        start = newEndpoints.xy;
        end = newEndpoints.zw;
        // Transform the stops into the range defined by the new endpoints.
        newStops[0] = 0;
        if (count > 2)
        {
            float m = 1.f / (lastStop - firstStop);
            float a = -firstStop * m;
            for (size_t i = 1; i < count - 1; ++i)
            {
                newStops[i] = std::clamp(stops[i] * m + a, newStops[i - 1], 1.f);
            }
        }
        newStops[count - 1] = 1;
    }

    return PLSGradient::MakeLinear(std::move(newColors),
                                   std::move(newStops),
                                   count,
                                   math::bit_cast<Vec2D>(start),
                                   math::bit_cast<Vec2D>(end));
}

rcp<RenderShader> PLSFactory::makeRadialGradient(float cx,
                                                 float cy,
                                                 float radius,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count)
{
    PLSGradDataArray<ColorInt> newColors(colors, count);
    PLSGradDataArray<float> newStops(stops, count);

    // If the stops don't end on 1, scale gradient so they do. This allows us to take better
    // advantage of the gradient's full range of pixels in the texture.
    //
    // TODO: If we want to take full advantage of the gradient texture pixels, we could add an inner
    // radius that specifies where t=0 begins (instead of assuming it begins at the center).
    float lastStop = stops[count - 1];
    if (lastStop != 1)
    {
        // Scale the radius to align with the final stop.
        radius *= lastStop;
        // Scale the stops into the range defined by the new radius.
        float inverseLastStop = 1.f / lastStop;
        for (size_t i = 0; i < count - 1; ++i)
        {
            newStops[i] = stops[i] * inverseLastStop;
        }
        newStops[count - 1] = 1;
    }

    return PLSGradient::MakeRadial(std::move(newColors),
                                   std::move(newStops),
                                   count,
                                   {cx, cy},
                                   radius);
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

std::unique_ptr<RenderImage> PLSFactory::decodeImage(Span<const uint8_t>) { return nullptr; }
} // namespace rive::pls
