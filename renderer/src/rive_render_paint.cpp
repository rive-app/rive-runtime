/*
 * Copyright 2022 Rive
 */

#include "rive_render_paint.hpp"
#include "gradient.hpp"

namespace rive
{
RiveRenderPaint::RiveRenderPaint() {}

RiveRenderPaint::~RiveRenderPaint() {}

void RiveRenderPaint::color(ColorInt color)
{
    m_paintType = gpu::PaintType::solidColor;
    m_simpleValue.color = color;
    m_gradient.reset();
    m_imageTexture.reset();
}

void RiveRenderPaint::shader(rcp<RenderShader> shader)
{
    m_gradient = static_rcp_cast<gpu::Gradient>(std::move(shader));
    m_paintType =
        m_gradient ? m_gradient->paintType() : gpu::PaintType::solidColor;
    // m_simpleValue.colorRampLocation is unused at this level. A new location
    // for a this gradient's color ramp will decided by the render context every
    // frame.
    m_simpleValue.color = 0xff000000;
    m_imageTexture.reset();
}

void RiveRenderPaint::image(rcp<const gpu::Texture> imageTexture, float opacity)
{
    m_paintType = gpu::PaintType::image;
    m_simpleValue.imageOpacity = opacity;
    m_gradient.reset();
    m_imageTexture = std::move(imageTexture);
}

void RiveRenderPaint::clipUpdate(uint32_t outerClipID)
{
    m_paintType = gpu::PaintType::clipUpdate;
    m_simpleValue.outerClipID = outerClipID;
    m_gradient.reset();
    m_imageTexture.reset();
}

bool RiveRenderPaint::getIsOpaque() const
{
    if (m_feather != 0)
    {
        return false;
    }
    if (m_blendMode != BlendMode::srcOver)
    {
        return false;
    }
    switch (m_paintType)
    {
        case gpu::PaintType::solidColor:
            return colorAlpha(m_simpleValue.color) == 0xff;
        case gpu::PaintType::linearGradient:
        case gpu::PaintType::radialGradient:
            return m_gradient->isOpaque();
        case gpu::PaintType::image:
        case gpu::PaintType::clipUpdate:
            return false;
    }
    RIVE_UNREACHABLE();
}
} // namespace rive
