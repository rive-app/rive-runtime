#ifndef _RIVE_TEXT_STYLE_GEOMETRY_HPP_
#define _RIVE_TEXT_STYLE_GEOMETRY_HPP_
#include "rive/refcnt.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include <map>
#include <vector>

namespace rive
{
class Factory;
class RawPath;
class RenderPaint;

/// The glyphs one Text draws with one text style: the outlines of every glyph
/// (for effects and feathers that need the whole shape), bucketed again by
/// glyph opacity, and the render paints those translucent buckets draw with.
/// A text style's paint owns one for the Text it belongs to; a Text owns one
/// per shared global style it draws, so the style's paint never holds any
/// one Text's glyphs.
class TextStyleGeometry
{
public:
  TextStyleGeometry();

  /// Adds a glyph outline drawn at [opacity]. Returns true when it's the
  /// first one since the last [rewind].
  bool addPath(const RawPath& rawPath, float opacity);
  void rewind();

  bool hasContents() const { return m_hasContents; }
  ShapePaintPath* path() { return &m_path; }
  std::map<float, ShapePaintPath>& opacityPaths() { return m_opacityPaths; }

  /// Makes sure there's a pooled render paint for every opacity bucket.
  void reservePaints(Factory* factory);
  /// The render paint for the [index]th pooled draw (see reservePaints).
  RenderPaint* pooledPaint(size_t index) { return m_paintPool[index].get(); }

private:
  std::map<float, ShapePaintPath> m_opacityPaths;
  std::vector<rcp<RenderPaint>> m_paintPool;
  ShapePaintPath m_path;
  bool m_hasContents = false;
};
} // namespace rive

#endif
