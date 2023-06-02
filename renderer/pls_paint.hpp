/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include "rive/renderer.hpp"
#include <array>

namespace rive::pls
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
class PLSGradient : public RenderShader
{
public:
    static rcp<PLSGradient> MakeLinear(PLSGradDataArray<ColorInt>&& colors, // [count]
                                       PLSGradDataArray<float>&& stops,     // [count]
                                       size_t count,
                                       Vec2D start,
                                       Vec2D end);

    static rcp<PLSGradient> MakeRadial(PLSGradDataArray<ColorInt>&& colors, // [count]
                                       PLSGradDataArray<float>&& stops,     // [count]
                                       size_t count,
                                       Vec2D center,
                                       float radius);

    PaintType paintType() const { return m_paintType; }
    const float* coeffs() const { return m_coeffs.data(); }
    const ColorInt* colors() const { return m_colors.get(); }
    const float* stops() const { return m_stops.get(); }
    int count() const { return m_count; }

private:
    PLSGradient(PaintType paintType,
                PLSGradDataArray<ColorInt>&& colors, // [count]
                PLSGradDataArray<float>&& stops,     // [count]
                size_t count) :
        m_paintType(paintType),
        m_colors(std::move(colors)),
        m_stops(std::move(stops)),
        m_count(count)
    {
        assert(paintType == PaintType::linearGradient || paintType == PaintType::radialGradient);
    }

    PaintType m_paintType;
    PLSGradDataArray<ColorInt> m_colors;
    PLSGradDataArray<float> m_stops;
    size_t m_count;
    std::array<float, 3> m_coeffs;
};

// RenderPaint implementation for Rive's pixel local storage renderer.
class PLSPaint : public RenderPaint
{
public:
    void style(RenderPaintStyle style) override { m_stroked = style == RenderPaintStyle::stroke; }
    void color(ColorInt color) override
    {
        m_gradient.reset();
        m_color = color;
    }
    void thickness(float thickness) override { m_thickness = fabsf(thickness); }
    void join(StrokeJoin join) override { m_join = join; }
    void cap(StrokeCap cap) override { m_cap = cap; }
    void blendMode(BlendMode mode) override;                  // Set a rive BlendMode.
    void blendMode(PLSBlendMode mode) { m_blendMode = mode; } // Set a pls BlendMode.
    void shader(rcp<RenderShader> shader) override;
    void invalidateStroke() override {}

    PaintType getType() const
    {
        return m_gradient ? m_gradient->paintType() : PaintType::solidColor;
    }
    bool getIsStroked() const { return m_stroked; }
    ColorInt getColor() const { return m_color; }
    float getThickness() const { return m_thickness; }
    const PLSGradient* getGradient() const { return m_gradient.get(); }
    StrokeJoin getJoin() const { return m_join; }
    StrokeCap getCap() const { return m_cap; }
    PLSBlendMode getBlendMode() const { return m_blendMode; }

private:
    bool m_stroked = false;
    ColorInt m_color = 0xff000000;
    rcp<PLSGradient> m_gradient;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::bevel;
    StrokeCap m_cap = StrokeCap::butt;
    PLSBlendMode m_blendMode = PLSBlendMode::srcOver;
};
} // namespace rive::pls
