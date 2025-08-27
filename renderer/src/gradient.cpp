/*
 * Copyright 2022 Rive
 */

#include "gradient.hpp"

namespace rive::gpu
{
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

rcp<Gradient> Gradient::MakeLinear(float sx,
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
    GradDataArray<ColorInt> newColors(colors, count);
    GradDataArray<float> newStops(stops, count);

    // If the stops don't begin and end on 0 and 1, transform the gradient so
    // they do. This allows us to take full advantage of the gradient's range of
    // pixels in the texture.
    float firstStop = stops[0];
    float lastStop = stops[count - 1];
    if ((firstStop != 0 || lastStop != 1) &&
        lastStop - firstStop > math::EPSILON)
    {
        // Tighten the endpoints to align with the mininum and maximum gradient
        // stops.
        float4 newEndpoints =
            simd::precise_mix(start.xyxy,
                              end.xyxy,
                              float4{firstStop, firstStop, lastStop, lastStop});
        start = newEndpoints.xy;
        end = newEndpoints.zw;
        newStops[0] = 0;
        newStops[count - 1] = 1;
        if (count > 2)
        {
            // Transform the stops into the range defined by the new endpoints.
            float m = 1.f / (lastStop - firstStop);
            float a = -firstStop * m;
            for (size_t i = 1; i < count - 1; ++i)
            {
                newStops[i] = stops[i] * m + a;
            }

            // Clamp the interior stops so they remain monotonically increasing.
            // newStops[0] and newStops[count - 1] are already 0 and 1, so this
            // also ensures they stay within 0..1.
            for (size_t i = 1; i < count - 1; ++i)
            {
                newStops[i] = fmaxf(newStops[i - 1], newStops[i]);
            }
            for (size_t i = count - 2; i != 0; --i)
            {
                newStops[i] = fminf(newStops[i], newStops[i + 1]);
            }
        }
        assert(validate_gradient_stops(newColors.get(), newStops.get(), count));
    }

    float2 v = end - start;
    v *= 1.f / simd::dot(v, v); // dot(v, end - start) == 1
    return rcp(new Gradient(gpu::PaintType::linearGradient,
                            std::move(newColors),
                            std::move(newStops),
                            count,
                            v.x,
                            v.y,
                            -simd::dot(v, start)));
}

rcp<Gradient> Gradient::MakeRadial(float cx,
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

    GradDataArray<ColorInt> newColors(colors, count);
    GradDataArray<float> newStops(stops, count);

    // If the stops don't end on 1, scale the gradient so they do. This allows
    // us to take better advantage of the gradient's full range of pixels in the
    // texture.
    //
    // TODO: If we want to take full advantage of the gradient texture pixels,
    // we could add an inner radius that specifies where t=0 begins (instead of
    // assuming it begins at the center).
    float lastStop = stops[count - 1];
    if (lastStop != 1 && lastStop > math::EPSILON)
    {
        // Update the gradient to finish on 1.
        newStops[count - 1] = 1;

        // Scale the radius to align with the final stop.
        radius *= lastStop;

        // Scale the stops into the range defined by the new radius.
        float inverseLastStop = 1.f / lastStop;
        for (size_t i = 0; i < count - 1; ++i)
        {
            newStops[i] = stops[i] * inverseLastStop;
        }

        if (count > 1)
        {
            // Clamp the stops so they remain monotonically increasing.
            // newStops[count - 1] is already 1, so this also ensures they stay
            // within 0..1.
            newStops[0] = fmaxf(0, newStops[0]);
            for (size_t i = 1; i < count - 1; ++i)
            {
                newStops[i] = fmaxf(newStops[i - 1], newStops[i]);
            }
            for (size_t i = count - 2; i != -1; --i)
            {
                newStops[i] = fminf(newStops[i], newStops[i + 1]);
            }
        }

        assert(validate_gradient_stops(newColors.get(), newStops.get(), count));
    }

    return rcp(new Gradient(gpu::PaintType::radialGradient,
                            std::move(newColors),
                            std::move(newStops),
                            count,
                            cx,
                            cy,
                            radius));
}

bool Gradient::isOpaque() const
{
    if (m_isOpaque == gpu::TriState::unknown)
    {
        ColorInt allColors = ~0;
        for (int i = 0; i < m_count; ++i)
        {
            allColors &= m_colors[i];
        }
        m_isOpaque = colorAlpha(allColors) == 0xff ? gpu::TriState::yes
                                                   : gpu::TriState::no;
    }
    return m_isOpaque == gpu::TriState::yes;
}

} // namespace rive::gpu
