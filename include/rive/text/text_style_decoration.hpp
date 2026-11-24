#ifndef _RIVE_TEXT_STYLE_DECORATION_HPP_
#define _RIVE_TEXT_STYLE_DECORATION_HPP_
#include "rive/generated/text/text_style_decoration_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text_selection_path.hpp"
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
  /// from [font]'s metrics at [fontSize] relative to [baseline].
  void addGlyph(
      float left,
      float right,
      float baseline,
      const Font* font,
      float fontSize);
  void updatePath();
  void draw(Renderer* renderer, const Mat2D& worldTransform);

  // Implemented for ShapePaintContainer.
  const Mat2D& shapeWorldTransform() const override;
  Component* pathBuilder() override;
  ShapePaintPath* localPath() override { return &m_path; }
  ShapePaintPath* localClockwisePath() override { return &m_path; }

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

  std::vector<AABB> m_rects;
  // Each glyph adds its own segment; TextSelectionPath joins the abutting
  // ones into a single contour per line, wound clockwise (see
  // TextStyleBackground).
  TextSelectionPath m_path{true, FillRule::clockwise};
};
} // namespace rive

#endif
