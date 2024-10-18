/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer.hpp"

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
    void blendMode(BlendMode mode) override { m_blendMode = mode; }
    void shader(rcp<RenderShader> shader) override;
    void image(rcp<const gpu::Texture>, float opacity);
    void clipUpdate(uint32_t outerClipID);
    void invalidateStroke() override {}

    gpu::PaintType getType() const { return m_paintType; }
    bool getIsStroked() const { return m_stroked; }
    ColorInt getColor() const { return m_simpleValue.color; }
    float getThickness() const { return m_thickness; }
    const gpu::Gradient* getGradient() const { return m_gradient.get(); }
    const gpu::Texture* getImageTexture() const { return m_imageTexture.get(); }
    float getImageOpacity() const { return m_simpleValue.imageOpacity; }
    float getOuterClipID() const { return m_simpleValue.outerClipID; }
    StrokeJoin getJoin() const { return m_join; }
    StrokeCap getCap() const { return m_cap; }
    BlendMode getBlendMode() const { return m_blendMode; }
    gpu::SimplePaintValue getSimpleValue() const { return m_simpleValue; }
    bool getIsOpaque() const;

private:
    gpu::PaintType m_paintType = gpu::PaintType::solidColor;
    gpu::SimplePaintValue m_simpleValue;
    rcp<const gpu::Gradient> m_gradient;
    rcp<const gpu::Texture> m_imageTexture;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    BlendMode m_blendMode = BlendMode::srcOver;
    bool m_stroked = false;
};
} // namespace rive
