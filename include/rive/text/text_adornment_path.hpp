#ifndef _RIVE_TEXT_ADORNMENT_PATH_HPP_
#define _RIVE_TEXT_ADORNMENT_PATH_HPP_
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/text/text_selection_path.hpp"
#include <memory>
#include <vector>

namespace rive
{
class Factory;
class Renderer;
class ShapePaint;

/// The rects a TextStyleBackground or TextStyleDecoration covers in one
/// layout, and the path they make. Rects join into one contour per line.
/// A glyph a text modifier moves, turns, scales or fades adds its rect apart
/// instead, mapped as the modifier maps the glyph and drawn at its opacity,
/// so text without modifiers builds and draws exactly as it would without
/// any of that.
class TextAdornmentPath
{
public:
  TextAdornmentPath();
  ~TextAdornmentPath();

  void reset();
  /// A rect of a glyph no modifier changed.
  void addRect(const AABB& rect) { m_rects.push_back(rect); }
  /// A rect of a glyph a modifier changed: [transform] (null for none) maps
  /// it in the Text's glyph space, as the glyph's outline is mapped, and it
  /// draws at [opacity] (nothing at 0).
  void addModifiedRect(const AABB& rect, const Mat2D* transform, float opacity);
  bool empty() const;
  /// Builds the path from the rects, rounding their corners by
  /// [cornerRadius].
  void update(float cornerRadius);
  /// Every rect's contours, as stroke effects and feathers read them.
  ShapePaintPath* path();
  /// Draws [shapePaint] over the rects: in one draw, unless a modifier faded
  /// some glyphs, which then draw through pooled render paints at their
  /// opacity, in the blend mode [shapePaint] takes ([parentMode] and
  /// [parentAdditiveAmount] when it inherits one). A PaintImage fits
  /// [imageBounds] (the path's own bounds when null).
  void draw(
      Renderer* renderer,
      ShapePaint* shapePaint,
      const Mat2D& worldTransform,
      Factory* factory,
      BlendMode parentMode,
      uint8_t parentAdditiveAmount,
      const AABB* imageBounds);

private:
  struct Modified;
  std::vector<AABB> m_rects;
  // Each glyph adds its own rect; TextSelectionPath joins the abutting ones
  // into a single contour per line, winding outer contours clockwise and
  // holes counter-clockwise, so the clockwise fill rule is accurate (and
  // the renderer only feathers fills whose path is clockwise).
  TextSelectionPath m_path{true, FillRule::clockwise};
  // The rects of glyphs a modifier changed, and the paths built with them.
  // Null until a modifier first changes one.
  std::unique_ptr<Modified> m_modified;
};
} // namespace rive

#endif
