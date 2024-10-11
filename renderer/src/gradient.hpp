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
template <typename T> class GradDataArray
{
public:
    static_assert(std::is_pod_v<T>);

    GradDataArray(const T data[], size_t count)
    {
        m_data =
            count <= m_localData.size() ? m_localData.data() : new T[count];
        memcpy(m_data, data, count * sizeof(T));
    }

    GradDataArray(GradDataArray&& other)
    {
        if (other.m_data == other.m_localData.data())
        {
            m_localData = other.m_localData;
            m_data = m_localData.data();
        }
        else
        {
            m_data = other.m_data;
            other.m_data =
                other.m_localData.data(); // Don't delete[] other.m_data.
        }
    }

    ~GradDataArray()
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
class Gradient : public lite_rtti_override<RenderShader, Gradient>
{
public:
    static rcp<Gradient> MakeLinear(float sx,
                                    float sy,
                                    float ex,
                                    float ey,
                                    const ColorInt colors[], // [count]
                                    const float stops[],     // [count]
                                    size_t count);

    static rcp<Gradient> MakeRadial(float cx,
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
    Gradient(PaintType paintType,
             GradDataArray<ColorInt>&& colors, // [count]
             GradDataArray<float>&& stops,     // [count]
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
        assert(paintType == gpu::PaintType::linearGradient ||
               paintType == gpu::PaintType::radialGradient);
    }

    PaintType m_paintType; // Specifically, linearGradient or radialGradient.
    GradDataArray<ColorInt> m_colors;
    GradDataArray<float> m_stops;
    size_t m_count;
    std::array<float, 3> m_coeffs;
    mutable gpu::TriState m_isOpaque = gpu::TriState::unknown;
};

} // namespace rive::gpu
