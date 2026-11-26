#ifndef _RIVE_TEXT_STYLE_DECORATION_HPP_
#define _RIVE_TEXT_STYLE_DECORATION_HPP_
#include "rive/generated/text/text_style_decoration_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text_adornment_path.hpp"
#include <vector>

namespace rive
{
class Font;
class TextStylePaint;

enum class TextDecorationType : uint8_t
{
  underline = 0,
  strikethrough = 1,
};

/// Paints a line under (underline) or through (strikethrough) the glyphs of
/// every run using the owning TextStylePaint, with its own fills/strokes.
class TextStyleDecoration : public TextStyleDecorationBase,
                            public ShapePaintContainer
{
public:
  StatusCode onAddedDirty(CoreContext* context) override;
  void buildDependencies() override;
#ifdef WITH_RIVE_EDITOR
  void editorParentChanged(ContainerComponent* from, ContainerComponent* to)
      override;
#endif

  TextDecorationType decorationType() const
  {
    return static_cast<TextDecorationType>(decorationTypeValue());
  }

  /// Underlines draw behind the glyphs, strikethroughs over them.
  bool drawsOverGlyphs() const
  {
    return decorationType() == TextDecorationType::strikethrough;
  }

  void resetPath();
  /// Adds the line under/through one glyph spanning [left, right], placed
  /// from [font]'s metrics at [fontSize] relative to [baseline]. A glyph a
  /// text modifier changed passes the [transform] (null for none) and
  /// [opacity] the modifier gave it (see TextAdornmentPath::addModifiedRect).
  void addGlyph(
      float left,
      float right,
      float baseline,
      const Font* font,
      float fontSize,
      const Mat2D* transform = nullptr,
      float opacity = 1.0f);
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
  void decorationTypeValueChanged() override;
  void thicknessChanged() override;
  void offsetChanged() override;

private:
  Artboard* getArtboard() override { return artboard(); }
  TextStylePaint* style() const;
  void markTextPaintDirty();
  /// Rebuilds whatever draws with style: its Text, or for a global text
  /// style (parented to its style artboard) the Texts its owner feeds.
  static void markStyleDirty(TextStylePaint* style);

  // Each glyph adds its own segment; they join into a single contour per
  // line (see TextAdornmentPath).
  TextAdornmentPath m_adornment;
};
} // namespace rive

#endif
