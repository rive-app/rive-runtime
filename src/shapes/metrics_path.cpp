#include "rive/shapes/metrics_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include <iostream>

using namespace rive;

void MetricsPath::rewind()
{
    for (auto ptr : m_Paths)
    {
        delete ptr;
    }
    m_Paths.clear();
    m_Contour.reset(nullptr);
    m_RawPath.rewind();
    m_ComputedLengthTransform = Mat2D();
    m_ComputedLength = 0;
}

MetricsPath::~MetricsPath() { rewind(); }

void MetricsPath::addPath(CommandPath* path, const Mat2D& transform)
{
    MetricsPath* metricsPath = static_cast<MetricsPath*>(path);
    m_ComputedLength += metricsPath->computeLength(transform);
    // We need to copy the data to avoid contention between multiple uses of the same path
    // for example when the same path is added as localPath and worldPath
    auto metricsPathCopy = new OnlyMetricsPath();
    metricsPathCopy->m_Contour = metricsPath->m_Contour;
    metricsPathCopy->m_RawPath = metricsPath->m_RawPath;
    metricsPathCopy->m_ComputedLength = metricsPath->m_ComputedLength;
    m_Paths.emplace_back(metricsPathCopy);
}

RawPath::Iter MetricsPath::addToRawPath(RawPath& rawPath, const Mat2D& transform) const
{
    return rawPath.addPath(m_RawPath, &transform);
}

void MetricsPath::moveTo(float x, float y)
{
    assert(m_RawPath.points().size() == 0);
    m_RawPath.move({x, y});
}

void MetricsPath::lineTo(float x, float y) { m_RawPath.line({x, y}); }

void MetricsPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    m_RawPath.cubic({ox, oy}, {ix, iy}, {x, y});
}

void MetricsPath::close()
{
    // Should we pass the close() to our m_RawPath ???
}

float MetricsPath::computeLength(const Mat2D& transform)
{
    // Only compute if our pre-computed length is not valid
    if (!m_Contour || transform != m_ComputedLengthTransform)
    {
        m_ComputedLengthTransform = transform;
        m_Contour = ContourMeasureIter(m_RawPath * transform).next();
        m_ComputedLength = m_Contour ? m_Contour->length() : 0;
    }
    return m_ComputedLength;
}

void MetricsPath::trim(float startLength, float endLength, bool moveTo, RawPath* result)
{
    assert(endLength >= startLength);
    if (!m_Paths.empty())
    {
        m_Paths.front()->trim(startLength, endLength, moveTo, result);
        return;
    }
    if (!m_Contour)
    {
        // All the contours were 0 length, so there's nothing to segment.
        return;
    }
    // TODO: if we can change the signature of MetricsPath and/or trim() to speak native
    //       rawpaths, we wouldn't need this temporary copy (since ContourMeasure speaks
    //       native rawpaths).
    RawPath tmp;
    m_Contour->getSegment(startLength, endLength, result, moveTo);
}

RenderMetricsPath::RenderMetricsPath(rcp<RenderPath> path) : m_RenderPath(std::move(path)) {}

void RenderMetricsPath::addPath(CommandPath* path, const Mat2D& transform)
{
    MetricsPath::addPath(path, transform);
    m_RenderPath->addPath(path->renderPath(), transform);
}

void RenderMetricsPath::rewind()
{
    MetricsPath::rewind();
    m_RenderPath->rewind();
}

void RenderMetricsPath::moveTo(float x, float y)
{
    MetricsPath::moveTo(x, y);
    m_RenderPath->moveTo(x, y);
}

void RenderMetricsPath::lineTo(float x, float y)
{
    MetricsPath::lineTo(x, y);
    m_RenderPath->lineTo(x, y);
}

void RenderMetricsPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    MetricsPath::cubicTo(ox, oy, ix, iy, x, y);
    m_RenderPath->cubicTo(ox, oy, ix, iy, x, y);
}

void RenderMetricsPath::close()
{
    MetricsPath::close();
    m_RenderPath->close();
}

void RenderMetricsPath::fillRule(FillRule value) { m_RenderPath->fillRule(value); }
