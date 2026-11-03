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
  bool addPath(
      const RawPath& rawPath,
      float opacity,
      const Mat2D* transform = nullptr);
  /// Makes room for glyph outlines totaling [verbs] verbs and [points]
  /// points, so adding them doesn't grow the paths an outline at a time.
  /// [allOpaque]: every one of them is added at full opacity, so the opaque
  /// bucket gets the room too.
  void reserve(size_t verbs, size_t points, bool allOpaque);
  void rewind();

  bool hasContents() const { return m_hasContents; }
  ShapePaintPath* path() { return &m_path; }
  std::map<float, ShapePaintPath>& opacityPaths() { return m_opacityPaths; }

  /// Makes sure there's a pooled render paint for every opacity bucket.
  void reservePaints(Factory* factory);
  /// The render paint for the [index]th pooled draw (see reservePaints).
  RenderPaint* pooledPaint(size_t index) { return m_paintPool[index].get(); }

private:
  ShapePaintPath& opacityPath(float opacity);

  std::map<float, ShapePaintPath> m_opacityPaths;
  // The opaque bucket, kept with its storage across a rewind: most Texts
  // draw every glyph opaque, and the next build fills it again. It draws
  // through a new render path, as a new bucket did.
  std::map<float, ShapePaintPath>::node_type m_spareOpaquePath;
  std::vector<rcp<RenderPaint>> m_paintPool;
  ShapePaintPath m_path;
  bool m_hasContents = false;
};
} // namespace rive

#endif
