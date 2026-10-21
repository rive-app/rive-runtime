#include "rive/text/text_style_geometry.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"

using namespace rive;

TextStyleGeometry::TextStyleGeometry() : m_path(true, FillRule::clockwise) {}

void TextStyleGeometry::rewind()
{
  m_path.rewind();
  m_hasContents = false;
  m_opacityPaths.clear();
}

bool TextStyleGeometry::addPath(const RawPath& rawPath, float opacity)
{
  bool hadContents = m_hasContents;
  m_hasContents = true;
  if (opacity > 0.0f)
  {
    // m_path contains everything, so inner feather bounds can work.
    m_path.addPathClockwise(rawPath);

    // Bucket by opacity
    auto itr = m_opacityPaths.find(opacity);
    ShapePaintPath* shapePaintPath = nullptr;
    if (itr != m_opacityPaths.end())
    {
      shapePaintPath = &itr->second;
    }
    else
    {
      m_opacityPaths[opacity] = ShapePaintPath(true, FillRule::clockwise);
      shapePaintPath = &m_opacityPaths.at(opacity);
    }
    shapePaintPath->addPathClockwise(rawPath);
  }
  return !hadContents;
}

void TextStyleGeometry::reservePaints(Factory* factory)
{
  if (m_paintPool.size() < m_opacityPaths.size())
  {
    m_paintPool.reserve(m_opacityPaths.size());
    while (m_paintPool.size() < m_opacityPaths.size())
    {
      m_paintPool.emplace_back(factory->makeRenderPaint());
    }
  }
}
