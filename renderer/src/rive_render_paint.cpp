/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/rive_render_image.hpp"
#include "rive_render_paint.hpp"
#include "gradient.hpp"

namespace rive
{
RiveRenderPaint::RiveRenderPaint() {}

RiveRenderPaint::~RiveRenderPaint() {}

rcp<RiveRenderPaint> RiveRenderPaint::clone() const
{
    auto r = make_rcp<RiveRenderPaint>();
    r->m_data = m_data;
    return r;
}

void RiveRenderPaint::color(ColorInt color)
{
    m_data.m_paintType = gpu::PaintType::solidColor;
    m_data.m_simpleValue.color = color;
    m_data.m_gradient.reset();
}

void RiveRenderPaint::shader(rcp<RenderShader> shader)
{
    m_data.m_gradient = static_rcp_cast<gpu::Gradient>(std::move(shader));
    m_data.m_paintType = m_data.m_gradient ? m_data.m_gradient->paintType()
                                           : gpu::PaintType::solidColor;
    // m_simpleValue.colorRampLocation is unused at this level. A new location
    // for a this gradient's color ramp will decided by the render context every
    // frame.
    m_data.m_simpleValue.color = 0xff000000;
}

rcp<gpu::Gradient> RiveRenderPaint::getGradientWithOpacity(float opacity) const
{
    if (m_data.m_gradient)
    {
        return m_data.m_gradient->getModulated(opacity);
    }
    return nullptr;
}

void RiveRenderPaint::modulatedImage(const RenderImage* renderImage,
                                     ImageSampler sampler,
                                     const Mat2D& matrix)
{
    if (renderImage == nullptr)
    {
        m_data.m_imageTexture = nullptr;
        return;
    }

    m_data.m_imageSampler = sampler;
    m_data.m_imageTransform = matrix;
    LITE_RTTI_CAST_OR_RETURN(riveImage, const RiveRenderImage*, renderImage);
    m_data.m_imageTexture = riveImage->refTexture();
}

void RiveRenderPaint::image(rcp<gpu::Texture> imageTexture, float opacity)
{
    m_data.m_paintType = gpu::PaintType::solidColor;
    m_data.m_simpleValue.color = colorModulateOpacity(0xFFFFFFFF, opacity);
    m_data.m_gradient.reset();
    m_data.m_imageTexture = std::move(imageTexture);
}

void RiveRenderPaint::clipUpdate(uint32_t outerClipID)
{
    m_data.m_paintType = gpu::PaintType::clipUpdate;
    m_data.m_simpleValue.outerClipID = outerClipID;
    m_data.m_gradient.reset();
    m_data.m_imageTexture.reset();
}

bool RiveRenderPaint::getIsOpaque() const
{
    if (m_data.m_feather != 0)
    {
        return false;
    }
    if (m_data.m_blendMode != BlendMode::srcOver)
    {
        return false;
    }
    if (m_data.m_additiveness != 0)
    {
        return false;
    }
    if (m_data.m_imageTexture != nullptr)
    {
        // We can't assume opacity with an image (as it might have non-1.0
        // alpha)
        return false;
    }

    switch (m_data.m_paintType)
    {
        case gpu::PaintType::solidColor:
            return colorAlpha(m_data.m_simpleValue.color) == 0xff;
        case gpu::PaintType::linearGradient:
        case gpu::PaintType::radialGradient:
            return m_data.m_gradient->isOpaque();
        case gpu::PaintType::clipUpdate:
            return false;
    }
    RIVE_UNREACHABLE();
}
} // namespace rive
