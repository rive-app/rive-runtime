/*
 * Copyright 2022 Rive
 */

#include "pls_paint.hpp"

#include "rive/pls/pls_image.hpp"

namespace rive::pls
{
PLSPaint::PLSPaint() {}

PLSPaint::~PLSPaint() {}

rcp<PLSGradient> PLSGradient::MakeLinear(float sx,
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

    float2 v = end - start;
    v *= 1.f / simd::dot(v, v); // dot(v, end - start) == 1
    return rcp(new PLSGradient(PaintType::linearGradient,
                               std::move(newColors),
                               std::move(newStops),
                               count,
                               v.x,
                               v.y,
                               -simd::dot(v, start)));
}

rcp<PLSGradient> PLSGradient::MakeRadial(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count)
{
    PLSGradDataArray<ColorInt> newColors(colors, count);
    PLSGradDataArray<float> newStops(stops, count);

    // If the stops don't end on 1, scale the gradient so they do. This allows us to take better
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

    return rcp(new PLSGradient(PaintType::radialGradient,
                               std::move(newColors),
                               std::move(newStops),
                               count,
                               cx,
                               cy,
                               radius));
}

void PLSPaint::blendMode(rive::BlendMode riveMode)
{
    m_blendMode = pls::BlendModeRiveToPLS(riveMode);
}

void PLSPaint::color(ColorInt color)
{
    m_imageTexture.reset();
    m_gradient.reset();
    m_color = color;
    m_paintType = PaintType::solidColor;
}

void PLSPaint::shader(rcp<RenderShader> shader)
{
    m_imageTexture.reset();
    m_gradient = static_rcp_cast<PLSGradient>(std::move(shader));
    m_paintType = m_gradient ? m_gradient->paintType() : PaintType::solidColor;
}

void PLSPaint::image(rcp<const PLSTexture> imageTexture, float opacity)
{
    m_imageOpacity = opacity;
    m_imageTexture = std::move(imageTexture);
    m_gradient.reset();
    m_paintType = PaintType::image;
}
} // namespace rive::pls
