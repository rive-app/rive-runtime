#include "rive/text/text_style_geometry.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"

using namespace rive;

TextStyleGeometry::TextStyleGeometry() : m_path(true, FillRule::clockwise) {}

void TextStyleGeometry::rewind()
{
  m_path.rewind();
  m_hasContents = false;
  if (m_spareOpaquePath.empty())
  {
    m_spareOpaquePath = m_opacityPaths.extract(1.0f);
    if (!m_spareOpaquePath.empty())
    {
      m_spareOpaquePath.mapped().rewindAsNew();
    }
  }
  m_opacityPaths.clear();
}

void TextStyleGeometry::reserve(size_t verbs, size_t points, bool allOpaque)
{
  m_path.mutableRawPath()->reserve(verbs, points);
  if (allOpaque)
  {
    opacityPath(1.0f).mutableRawPath()->reserve(verbs, points);
  }
}

ShapePaintPath& TextStyleGeometry::opacityPath(float opacity)
{
  auto itr = m_opacityPaths.find(opacity);
  if (itr != m_opacityPaths.end())
  {
    return itr->second;
  }
  if (opacity == 1.0f && !m_spareOpaquePath.empty())
  {
    return m_opacityPaths.insert(std::move(m_spareOpaquePath)).position->second;
  }
  return m_opacityPaths
      .emplace(opacity, ShapePaintPath(true, FillRule::clockwise))
      .first->second;
}

bool TextStyleGeometry::addPath(
    const RawPath& rawPath,
    float opacity,
    const Mat2D* transform)
{
  bool hadContents = m_hasContents;
  m_hasContents = true;
  if (opacity > 0.0f)
  {
    // Added clockwise, to both paths below: one winding check for both.
    const bool backwards = (transform == nullptr
                                ? rawPath.computeCoarseArea()
                                : rawPath.computeCoarseArea(*transform)) < 0;
    auto add = [&](ShapePaintPath& path) {
      if (backwards)
      {
        path.addPathBackwards(rawPath, transform);
      }
      else
      {
        path.addPath(rawPath, transform);
      }
    };

    // m_path contains everything, so inner feather bounds can work.
    add(m_path);

    // Bucket by opacity
    add(opacityPath(opacity));
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
      m_paintPool.push_back({factory->makeRenderPaint()});
    }
  }
}

RenderPaint* TextStyleGeometry::pooledPaint(size_t index, bool modulated)
{
  PooledPaint& pooled = m_paintPool[index];
  // Only cleared when one was left there: some backends record or forward
  // every call.
  if (pooled.modulated && !modulated)
  {
    pooled.paint->modulatedImage(nullptr, ImageSampler::LinearClamp(), Mat2D());
  }
  pooled.modulated = modulated;
  return pooled.paint.get();
}
