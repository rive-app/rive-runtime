/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/texture.hpp"

namespace rive::gpu
{
class Gradient;
}

namespace rive
{
// RenderPaint implementation for Rive's pixel local storage renderer.
class RiveRenderPaint : public LITE_RTTI_OVERRIDE(RenderPaint, RiveRenderPaint)
{
public:
    RiveRenderPaint();
    ~RiveRenderPaint();

    void style(RenderPaintStyle style) override
    {
        m_stroked = style == RenderPaintStyle::stroke;
    }
    void color(ColorInt color) override;
    void thickness(float thickness) override { m_thickness = fabsf(thickness); }
    void join(StrokeJoin join) override { m_join = join; }
    void cap(StrokeCap cap) override { m_cap = cap; }
    void feather(float feather) override { m_feather = fabsf(feather); }
    void blendMode(BlendMode mode) override { m_blendMode = mode; }
    void shader(rcp<RenderShader> shader) override;
    void image(rcp<gpu::Texture>, float opacity);
    void imageSampler(ImageSampler imageSampler)
    {
        m_imageSampler = imageSampler;
    }
    void clipUpdate(uint32_t outerClipID);
    void invalidateStroke() override {}

    gpu::PaintType getType() const { return m_paintType; }
    bool getIsStroked() const { return m_stroked; }
    ColorInt getColor() const { return m_simpleValue.color; }
    const gpu::Gradient* getGradient() const { return m_gradient.get(); }
    gpu::Texture* getImageTexture() const { return m_imageTexture.get(); }
    ImageSampler getImageSampler() const { return m_imageSampler; }
    float getImageOpacity() const { return m_simpleValue.imageOpacity; }
    float getOuterClipID() const { return m_simpleValue.outerClipID; }
    float getThickness() const { return m_thickness; }
    StrokeJoin getJoin() const
    {
        // Feathers ignore the join and always use round.
        return m_feather != 0 ? StrokeJoin::round : m_join;
    }
    StrokeCap getCap() const
    {
        // Feathers ignore the cap and always use round.
        return m_feather != .0 ? StrokeCap::round : m_cap;
    }
    float getFeather() const { return m_feather; }
    BlendMode getBlendMode() const { return m_blendMode; }
    gpu::SimplePaintValue getSimpleValue() const { return m_simpleValue; }
    bool getIsOpaque() const;

private:
    gpu::PaintType m_paintType = gpu::PaintType::solidColor;
    gpu::SimplePaintValue m_simpleValue;
    rcp<const gpu::Gradient> m_gradient;
    rcp<gpu::Texture> m_imageTexture;
    ImageSampler m_imageSampler = ImageSampler::LinearClamp();
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    float m_feather = 0;
    BlendMode m_blendMode = BlendMode::srcOver;
    bool m_stroked = false;
};
} // namespace rive
