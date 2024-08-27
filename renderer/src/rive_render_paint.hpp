/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer.hpp"
#include <array>

namespace rive::gpu
{
// Copies an array of colors or stops for a gradient.
// Stores the data locally if there are 4 values or fewer.
// Spills onto the heap if there are >4 values.
template <typename T> class PLSGradDataArray
{
public:
    static_assert(std::is_pod_v<T>);

    PLSGradDataArray(const T data[], size_t count)
    {
        m_data = count <= m_localData.size() ? m_localData.data() : new T[count];
        memcpy(m_data, data, count * sizeof(T));
    }

    PLSGradDataArray(PLSGradDataArray&& other)
    {
        if (other.m_data == other.m_localData.data())
        {
            m_localData = other.m_localData;
            m_data = m_localData.data();
        }
        else
        {
            m_data = other.m_data;
            other.m_data = other.m_localData.data(); // Don't delete[] other.m_data.
        }
    }

    ~PLSGradDataArray()
    {
        if (m_data != m_localData.data())
        {
            delete[] m_data;
        }
    }

    const T* get() const { return m_data; }
    const T operator[](size_t i) const { return m_data[i]; }
    T& operator[](size_t i) { return m_data[i]; }

private:
    std::array<T, 4> m_localData;
    T* m_data;
};

// RenderShader implementation for Rive's pixel local storage renderer.
class PLSGradient : public lite_rtti_override<RenderShader, PLSGradient>
{
public:
    static rcp<PLSGradient> MakeLinear(float sx,
                                       float sy,
                                       float ex,
                                       float ey,
                                       const ColorInt colors[], // [count]
                                       const float stops[],     // [count]
                                       size_t count);

    static rcp<PLSGradient> MakeRadial(float cx,
                                       float cy,
                                       float radius,
                                       const ColorInt colors[], // [count]
                                       const float stops[],     // [count]
                                       size_t count);

    PaintType paintType() const { return m_paintType; }
    const float* coeffs() const { return m_coeffs.data(); }
    const ColorInt* colors() const { return m_colors.get(); }
    const float* stops() const { return m_stops.get(); }
    size_t count() const { return m_count; }
    bool isOpaque() const;

private:
    PLSGradient(PaintType paintType,
                PLSGradDataArray<ColorInt>&& colors, // [count]
                PLSGradDataArray<float>&& stops,     // [count]
                size_t count,
                float coeffX,
                float coeffY,
                float coeffZ) :
        m_paintType(paintType),
        m_colors(std::move(colors)),
        m_stops(std::move(stops)),
        m_count(count),
        m_coeffs{coeffX, coeffY, coeffZ}
    {
        assert(paintType == PaintType::linearGradient || paintType == PaintType::radialGradient);
    }

    PaintType m_paintType; // Specifically, linearGradient or radialGradient.
    PLSGradDataArray<ColorInt> m_colors;
    PLSGradDataArray<float> m_stops;
    size_t m_count;
    std::array<float, 3> m_coeffs;
    mutable gpu::TriState m_isOpaque = gpu::TriState::unknown;
};

// RenderPaint implementation for Rive's pixel local storage renderer.
class RiveRenderPaint : public lite_rtti_override<RenderPaint, RiveRenderPaint>
{
public:
    RiveRenderPaint();
    ~RiveRenderPaint();

    void style(RenderPaintStyle style) override { m_stroked = style == RenderPaintStyle::stroke; }
    void color(ColorInt color) override;
    void thickness(float thickness) override { m_thickness = fabsf(thickness); }
    void join(StrokeJoin join) override { m_join = join; }
    void cap(StrokeCap cap) override { m_cap = cap; }
    void blendMode(BlendMode mode) override { m_blendMode = mode; }
    void shader(rcp<RenderShader> shader) override;
    void image(rcp<const PLSTexture>, float opacity);
    void clipUpdate(uint32_t outerClipID);
    void invalidateStroke() override {}

    PaintType getType() const { return m_paintType; }
    bool getIsStroked() const { return m_stroked; }
    ColorInt getColor() const { return m_simpleValue.color; }
    float getThickness() const { return m_thickness; }
    const PLSGradient* getGradient() const { return m_gradient.get(); }
    const PLSTexture* getImageTexture() const { return m_imageTexture.get(); }
    float getImageOpacity() const { return m_simpleValue.imageOpacity; }
    float getOuterClipID() const { return m_simpleValue.outerClipID; }
    StrokeJoin getJoin() const { return m_join; }
    StrokeCap getCap() const { return m_cap; }
    BlendMode getBlendMode() const { return m_blendMode; }
    gpu::SimplePaintValue getSimpleValue() const { return m_simpleValue; }
    bool getIsOpaque() const;

private:
    PaintType m_paintType = PaintType::solidColor;
    gpu::SimplePaintValue m_simpleValue;
    rcp<const PLSGradient> m_gradient;
    rcp<const PLSTexture> m_imageTexture;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    BlendMode m_blendMode = BlendMode::srcOver;
    bool m_stroked = false;
};
} // namespace rive::gpu
