#include "rive/shapes/shape_paint_path.hpp"
#include "rive/component.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

ShapePaintPath::ShapePaintPath(bool isLocal) : m_isLocal(isLocal) {}
ShapePaintPath::ShapePaintPath(bool isLocal, FillRule fillRule) :
    m_isLocal(isLocal), m_fillRule(fillRule)
{}

void ShapePaintPath::rewind()
{
    m_rawPath.rewind();
    m_renderPath = nullptr;
}

void ShapePaintPath::addPath(const RawPath& rawPath, const Mat2D* transform)
{
    auto iter = m_rawPath.addPath(rawPath, transform);
    m_rawPath.pruneEmptySegments(iter);
    m_renderPath = nullptr;
}

void ShapePaintPath::addPathClockwise(const RawPath& rawPath,
                                      const Mat2D* transform)
{
    float coarseArea = rawPath.computeCoarseArea();
    if (transform != nullptr)
    {
        coarseArea *= transform->determinant();
    }
    if (coarseArea < 0)
    {
        addPathBackwards(rawPath, transform);
    }
    else
    {
        addPath(rawPath, transform);
    }
}

void ShapePaintPath::addPathBackwards(const RawPath& rawPath,
                                      const Mat2D* transform)
{
    auto iter = m_rawPath.addPathBackwards(rawPath, transform);
    m_rawPath.pruneEmptySegments(iter);
    m_renderPath = nullptr;
}

RenderPath* ShapePaintPath::renderPath(const Component* component)
{
    assert(component != nullptr);
    if (!m_renderPath)
    {
        auto factory = component->artboard()->factory();
        m_renderPath = factory->makeRenderPath(m_rawPath, m_fillRule);
    }
    return m_renderPath.get();
}

RenderPath* ShapePaintPath::renderPath(Factory* factory)
{
    if (!m_renderPath)
    {
        m_renderPath = factory->makeRenderPath(m_rawPath, m_fillRule);
    }
    return m_renderPath.get();
}