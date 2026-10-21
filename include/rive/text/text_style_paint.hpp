#ifndef _RIVE_TEXT_STYLE_PAINT_HPP_
#define _RIVE_TEXT_STYLE_PAINT_HPP_
#include "rive/generated/text/text_style_paint_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/text/text_style_geometry.hpp"

namespace rive
{
class TextStyleBackground;

class TextStylePaint : public TextStylePaintBase, public ShapePaintContainer
{
public:
  TextStylePaint();
  bool addPath(const RawPath& rawPath, float opacity);
  void rewindPath();
  void draw(Renderer* renderer, const Mat2D& worldTransform);

  /// Draws [geometry] (some Text's glyphs) with this paint. A paint shared
  /// by several Texts draws each one's own geometry, modulated by that
  /// Text's opacity ([opacityModifier]), without touching its own render
  /// paints; 1 draws this paint's own glyphs as it always has.
  void drawGeometry(
      Renderer* renderer,
      const Mat2D& worldTransform,
      TextStyleGeometry& geometry,
      float opacityModifier);

  /// Whether this is the paint of a style instance every Text using it
  /// draws (see SharedTextStyle): it never holds any Text's glyphs, each
  /// Text draws its own geometry with it.
  bool isShared() const { return m_isShared; }
  void isShared(bool value) { m_isShared = value; }

  void background(TextStyleBackground* value) { m_background = value; }
  TextStyleBackground* background() const { return m_background; }

  /// A local paint resolves to itself.
  TextStylePaint* resolvedPaint() override { return this; }

  /// Returns the foreground color from the first solid fill, or black if
  /// none.
  ColorInt foregroundColor() const;

  // Implemented for ShapePaintContainer.
  const Mat2D& shapeWorldTransform() const override;
  Component* pathBuilder() override;
  ShapePaintPath* localPath() override { return m_geometry.path(); }
  ShapePaintPath* localClockwisePath() override { return m_geometry.path(); }
  Core* clone() const override;

private:
  Artboard* getArtboard() override { return artboard(); }
  // The glyphs of the Text this paint belongs to.
  TextStyleGeometry m_geometry;
  TextStyleBackground* m_background = nullptr;
  bool m_isShared = false;
};
} // namespace rive

#endif
