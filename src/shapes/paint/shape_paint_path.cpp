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
    m_isRenderPathDirty = true;
}

void ShapePaintPath::addPath(const RawPath& rawPath, const Mat2D* transform)
{
    auto iter = m_rawPath.addPath(rawPath, transform);
    m_rawPath.pruneEmptySegments(iter);
    m_isRenderPathDirty = true;
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
    m_isRenderPathDirty = true;
}

RenderPath* ShapePaintPath::renderPath(const Component* component)
{
    assert(component != nullptr);
    return renderPath(component->artboard()->factory());
}

RenderPath* ShapePaintPath::renderPath(Factory* factory)
{
    if (!m_renderPath)
    {
        m_renderPath = factory->makeEmptyRenderPath();
        m_renderPath->addRawPath(m_rawPath);
        m_renderPath->fillRule(m_fillRule);
        m_isRenderPathDirty = false;
    }
    else if (m_isRenderPathDirty)
    {
        m_renderPath->rewind();
        m_renderPath->addRawPath(m_rawPath);
        m_isRenderPathDirty = false;
    }

    return m_renderPath.get();
}