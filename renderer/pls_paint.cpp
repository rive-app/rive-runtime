/*
 * Copyright 2022 Rive
 */

#include "pls_paint.hpp"

#include "rive/pls/pls_image.hpp"

namespace rive::pls
{
PLSPaint::PLSPaint() {}

PLSPaint::~PLSPaint() {}

// Ensure the given gradient stops are in a format expected by PLS.
static bool validate_gradient_stops(const ColorInt colors[], // [count]
                                    const float stops[],     // [count]
                                    size_t count)
{
    // Stops cannot be empty.
    if (count == 0)
    {
        return false;
    }
    for (size_t i = 0; i < count; ++i)
    {
        // Stops must be finite, real numbers in the range [0, 1].
        if (!(0 <= stops[i] && stops[i] <= 1))
        {
            return false;
        }
    }
    for (size_t i = 1; i < count; ++i)
    {
        // Stops must be ordered.
        if (!(stops[i - 1] <= stops[i]))
        {
            return false;
        }
    }
    return true;
}

rcp<PLSGradient> PLSGradient::MakeLinear(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count)
{
    if (!validate_gradient_stops(colors, stops, count))
    {
        return nullptr;
    }

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
    if (!validate_gradient_stops(colors, stops, count))
    {
        return nullptr;
    }

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

bool PLSGradient::isOpaque() const
{
    if (m_isOpaque == pls::TriState::unknown)
    {
        ColorInt allColors = ~0;
        for (int i = 0; i < m_count; ++i)
        {
            allColors &= m_colors[i];
        }
        m_isOpaque = colorAlpha(allColors) == 0xff ? pls::TriState::yes : pls::TriState::no;
    }
    return m_isOpaque == pls::TriState::yes;
}

void PLSPaint::color(ColorInt color)
{
    m_paintType = PaintType::solidColor;
    m_simpleValue.color = color;
    m_gradient.reset();
    m_imageTexture.reset();
}

void PLSPaint::shader(rcp<RenderShader> shader)
{
    m_gradient = static_rcp_cast<PLSGradient>(std::move(shader));
    m_paintType = m_gradient ? m_gradient->paintType() : PaintType::solidColor;
    // m_simpleValue.colorRampLocation is unused at this level. A new location for a this gradient's
    // color ramp will decided by the render context every frame.
    m_simpleValue.color = 0xff000000;
    m_imageTexture.reset();
}

void PLSPaint::image(rcp<const PLSTexture> imageTexture, float opacity)
{
    m_paintType = PaintType::image;
    m_simpleValue.imageOpacity = opacity;
    m_gradient.reset();
    m_imageTexture = std::move(imageTexture);
}

void PLSPaint::clipUpdate(uint32_t outerClipID)
{
    m_paintType = PaintType::clipUpdate;
    m_simpleValue.outerClipID = outerClipID;
    m_gradient.reset();
    m_imageTexture.reset();
}

bool PLSPaint::getIsOpaque() const
{
    switch (m_paintType)
    {
        case pls::PaintType::solidColor:
            return colorAlpha(m_simpleValue.color) == 0xff;
        case pls::PaintType::linearGradient:
        case pls::PaintType::radialGradient:
            return m_gradient->isOpaque();
        case pls::PaintType::image:
        case pls::PaintType::clipUpdate:
            return false;
    }
    RIVE_UNREACHABLE();
}
} // namespace rive::pls
