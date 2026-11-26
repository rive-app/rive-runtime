#ifndef _RIVE_TEXT_STYLE_BACKGROUND_HPP_
#define _RIVE_TEXT_STYLE_BACKGROUND_HPP_
#include "rive/generated/text/text_style_background_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text_adornment_path.hpp"

namespace rive
{
class TextStylePaint;

/// Paints a joined rounded rectangle behind the glyphs of every run using the
/// owning TextStylePaint.
class TextStyleBackground : public TextStyleBackgroundBase,
                            public ShapePaintContainer
{
public:
  StatusCode onAddedDirty(CoreContext* context) override;
  void buildDependencies() override;

  void resetPath();
  void addRect(const AABB& rect);
  /// A rect of a glyph a text modifier changed (see
  /// TextAdornmentPath::addModifiedRect).
  void addModifiedRect(const AABB& rect, const Mat2D* transform, float opacity)
  {
    m_adornment.addModifiedRect(rect, transform, opacity);
  }
  void updatePath();
  /// Draws the rects of the last layout. A PaintImage on its paints fits
  /// [imageBounds], the drawing Text's layout box in the glyphs' space (see
  /// Text::glyphSpaceBounds), as the editor fits it.
  void draw(
      Renderer* renderer,
      const Mat2D& worldTransform,
      const AABB* imageBounds = nullptr);

  // Implemented for ShapePaintContainer.
  const Mat2D& shapeWorldTransform() const override;
  Component* pathBuilder() override;
  ShapePaintPath* localPath() override { return m_adornment.path(); }
  ShapePaintPath* localClockwisePath() override { return m_adornment.path(); }

protected:
  void cornerRadiusChanged() override;

private:
  Artboard* getArtboard() override { return artboard(); }
  TextStylePaint* style() const;

  // The glyph rects of the last layout, and the rounded path they join
  // into.
  TextAdornmentPath m_adornment;
};
} // namespace rive

#endif
