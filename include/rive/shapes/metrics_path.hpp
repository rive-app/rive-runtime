#ifndef _RIVE_METRICS_PATH_HPP_
#define _RIVE_METRICS_PATH_HPP_

#include "rive/command_path.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/vec2d.hpp"
#include <cassert>
#include <vector>

namespace rive
{

class MetricsPath : public CommandPath
{
private:
    RawPath m_RawPath; // temporary, until we build m_Contour
    rcp<ContourMeasure> m_Contour;
    std::vector<MetricsPath*> m_Paths;
    Mat2D m_ComputedLengthTransform;
    float m_ComputedLength = 0;

public:
    const std::vector<MetricsPath*>& paths() const { return m_Paths; }

    void addPath(CommandPath* path, const Mat2D& transform) override;
    void reset() override;
    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    void close() override;

    float length() const { return m_ComputedLength; }

    /// Add commands to the result RenderPath that will draw the segment
    /// from startLength to endLength of this MetricsPath. Requires
    /// computeLength be called prior to trimming.
    void trim(float startLength, float endLength, bool moveTo, RenderPath* result);

private:
    float computeLength(const Mat2D& transform);
};

class OnlyMetricsPath : public MetricsPath
{
public:
    void fillRule(FillRule value) override {}

    RenderPath* renderPath() override
    {
        // Should never be used for actual rendering.
        assert(false);
        return nullptr;
    }
};

class RenderMetricsPath : public MetricsPath
{
private:
    std::unique_ptr<RenderPath> m_RenderPath;

public:
    RenderMetricsPath(std::unique_ptr<RenderPath>);
    RenderPath* renderPath() override { return m_RenderPath.get(); }
    void addPath(CommandPath* path, const Mat2D& transform) override;

    void fillRule(FillRule value) override;
    void reset() override;
    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    void close() override;
};
} // namespace rive
#endif