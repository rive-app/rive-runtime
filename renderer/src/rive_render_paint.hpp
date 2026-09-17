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
        m_data.m_stroked = style == RenderPaintStyle::stroke;
    }
    void color(ColorInt color) override;
    void thickness(float thickness) override
    {
        m_data.m_thickness = fabsf(thickness);
    }
    void join(StrokeJoin join) override { m_data.m_join = join; }
    void cap(StrokeCap cap) override { m_data.m_cap = cap; }
    void feather(float feather) override { m_data.m_feather = fabsf(feather); }
    void strokePosition(StrokePosition pos) override
    {
        m_data.m_strokePosition = pos;
    }
    void additiveness(float additiveness) override
    {
        m_data.m_additiveness = additiveness;
    }
    void blendMode(BlendMode mode) override { m_data.m_blendMode = mode; }
    void shader(rcp<RenderShader> shader) override;

    void modulatedImage(const RenderImage*,
                        ImageSampler,
                        const Mat2D&) override;
    void image(rcp<gpu::Texture>, float opacity);
    void imageSampler(ImageSampler imageSampler)
    {
        m_data.m_imageSampler = imageSampler;
    }
    void clipUpdate(uint32_t outerClipID);
    void invalidateStroke() override {}

    gpu::PaintType getType() const { return m_data.m_paintType; }
    bool getIsStroked() const { return m_data.m_stroked; }
    ColorInt getColor() const { return m_data.m_simpleValue.color; }
    const gpu::Gradient* getGradient() const { return m_data.m_gradient.get(); }
    rcp<gpu::Gradient> getGradientWithOpacity(float opacity) const;
    gpu::Texture* getImageTexture() const
    {
        return m_data.m_imageTexture.get();
    }
    ImageSampler getImageSampler() const { return m_data.m_imageSampler; }
    float getOuterClipID() const { return m_data.m_simpleValue.outerClipID; }
    float getThickness() const { return m_data.m_thickness; }
    const Mat2D& getImageTransform() const { return m_data.m_imageTransform; }
    StrokeJoin getJoin() const
    {
        // Feathers ignore the join and always use round.
        return m_data.m_feather != 0.0f ? StrokeJoin::round : m_data.m_join;
    }
    StrokeCap getCap() const
    {
        // Feathers ignore the cap and always use round.
        return m_data.m_feather != 0.0f ? StrokeCap::round : m_data.m_cap;
    }

    StrokePosition getStrokePosition() const { return m_data.m_strokePosition; }
    float getFeather() const { return m_data.m_feather; }
    float getAdditiveness() const { return m_data.m_additiveness; }
    BlendMode getBlendMode() const { return m_data.m_blendMode; }
    gpu::SimplePaintValue getSimpleValue() const
    {
        return m_data.m_simpleValue;
    }
    bool getIsOpaque() const;

    StrokeParams getStrokeParams() const
    {
        return {getThickness(), getJoin(), getCap(), getStrokePosition()};
    }

    rcp<RiveRenderPaint> clone() const;

    // RiveRenderPaint-specific functionality to force a stroke to draw as if
    // the path's contours were closed - used for inner/outer strokes.
    void forceClosed(bool force) { m_data.m_forceClosed = force; }
    bool getForceClosed() const { return m_data.m_forceClosed; }

private:
    // Wrap all the internals in a struct to make the copy
    // constructor/assignment less error-prone.
    struct Data
    {
        gpu::PaintType m_paintType = gpu::PaintType::solidColor;
        gpu::SimplePaintValue m_simpleValue;
        rcp<const gpu::Gradient> m_gradient;
        rcp<gpu::Texture> m_imageTexture;
        ImageSampler m_imageSampler = ImageSampler::LinearClamp();
        float m_thickness = 1;
        StrokeJoin m_join = StrokeJoin::miter;
        StrokeCap m_cap = StrokeCap::butt;
        StrokePosition m_strokePosition = StrokePosition::center;
        float m_feather = 0;
        float m_additiveness = 0;
        BlendMode m_blendMode = BlendMode::srcOver;
        bool m_stroked = false;
        Mat2D m_imageTransform;
        bool m_forceClosed = false;
    };

    Data m_data;
};
} // namespace rive
