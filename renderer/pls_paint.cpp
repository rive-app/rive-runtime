/*
 * Copyright 2022 Rive
 */

#include "pls_paint.hpp"

namespace rive::pls
{
rcp<PLSGradient> PLSGradient::MakeLinear(PLSGradDataArray<ColorInt>&& colors, // [count]
                                         PLSGradDataArray<float>&& stops,     // [count]
                                         size_t count,
                                         Vec2D start,
                                         Vec2D end)
{
    rcp<PLSGradient> gradient(
        new PLSGradient(PaintType::linearGradient, std::move(colors), std::move(stops), count));
    // T = coeffs[0] * x + coeffs[1] * y + coeffs[2]
    Vec2D v = end - start;
    v *= 1.f / Vec2D::dot(v, v); // dot(v, end - start) == 1
    gradient->m_coeffs = {v.x, v.y, -Vec2D::dot(v, start)};
    return gradient;
}

rcp<PLSGradient> PLSGradient::MakeRadial(PLSGradDataArray<ColorInt>&& colors, // [count]
                                         PLSGradDataArray<float>&& stops,     // [count]
                                         size_t count,
                                         Vec2D center,
                                         float radius)
{
    rcp<PLSGradient> gradient(
        new PLSGradient(PaintType::radialGradient, std::move(colors), std::move(stops), count));
    // T = length(x - coeffs[0], y - coeffs[1]) / coeffs[2]
    gradient->m_coeffs = {center.x, center.y, radius};
    return gradient;
}

static PLSBlendMode blend_mode_rive_to_pls(rive::BlendMode riveMode)
{
    switch (riveMode)
    {
        case rive::BlendMode::srcOver:
            return PLSBlendMode::srcOver;
        case rive::BlendMode::screen:
            return PLSBlendMode::screen;
        case rive::BlendMode::overlay:
            return PLSBlendMode::overlay;
        case rive::BlendMode::darken:
            return PLSBlendMode::darken;
        case rive::BlendMode::lighten:
            return PLSBlendMode::lighten;
        case rive::BlendMode::colorDodge:
            return PLSBlendMode::colorDodge;
        case rive::BlendMode::colorBurn:
            return PLSBlendMode::colorBurn;
        case rive::BlendMode::hardLight:
            return PLSBlendMode::hardLight;
        case rive::BlendMode::softLight:
            return PLSBlendMode::softLight;
        case rive::BlendMode::difference:
            return PLSBlendMode::difference;
        case rive::BlendMode::exclusion:
            return PLSBlendMode::exclusion;
        case rive::BlendMode::multiply:
            return PLSBlendMode::multiply;
        case rive::BlendMode::hue:
            return PLSBlendMode::hue;
        case rive::BlendMode::saturation:
            return PLSBlendMode::saturation;
        case rive::BlendMode::color:
            return PLSBlendMode::color;
        case rive::BlendMode::luminosity:
            return PLSBlendMode::luminosity;
    }
    RIVE_UNREACHABLE();
}

void PLSPaint::blendMode(rive::BlendMode riveMode)
{
    m_blendMode = blend_mode_rive_to_pls(riveMode);
}

void PLSPaint::shader(rcp<RenderShader> shader)
{
    m_gradient = static_rcp_cast<PLSGradient>(std::move(shader));
}
} // namespace rive::pls
