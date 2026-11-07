#include "rive/text/text_style_paint.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_variation_helper.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

TextStylePaint::TextStylePaint() {}

void TextStylePaint::rewindPath() { m_geometry.rewind(); }

bool TextStylePaint::addPath(
    const RawPath& rawPath,
    float opacity,
    const Mat2D* transform)
{
  return m_geometry.addPath(rawPath, opacity, transform);
}

void TextStylePaint::draw(
    Renderer* renderer,
    const Mat2D& worldTransform,
    const AABB& imageBounds)
{
  drawGeometry(renderer, worldTransform, m_geometry, 1.0f, imageBounds);
}

void TextStylePaint::drawGeometry(
    Renderer* renderer,
    const Mat2D& worldTransform,
    TextStyleGeometry& geometry,
    float opacityModifier,
    const AABB& imageBounds)
{
  // A paint shared by several Texts (opacityModifier carries the drawing
  // Text's opacity) never has its own render paints touched: every bucket,
  // the opaque one included, draws through the geometry's pooled paints.
  const bool owned = opacityModifier == 1.0f;
  auto& opacityPaths = geometry.opacityPaths();
  for (auto shapePaint : m_ShapePaints)
  {
    if (!shapePaint->shouldDraw())
    {
      continue;
    }
    // A PaintImage fits every bucket to the same box, so the image lines up
    // across glyphs whatever their opacity.
    const bool modulated = shapePaint->paintImage() != nullptr;
    // A global text style's paint lives under the style Artboard, not a
    // Text; keep its shape paint's own (default) blend mode in that case.
    if (owned && parent()->is<Text>())
    {
      auto text = parent()->as<Text>();
      shapePaint->blendMode(text->blendMode(), text->additiveAmount());
    }

    auto drawPooled = [&](ShapePaintPath* path,
                          float opacity,
                          uint32_t paintIndex) {
      RenderPaint* renderPaint = geometry.pooledPaint(paintIndex, modulated);
      shapePaint->applyTo(renderPaint, opacity * opacityModifier);
      // Pooled paints are shared across shape paints; always set
      // feather and blend mode so one paint's settings don't leak into
      // the others. Pooled draws take the shape paint's own mode (a
      // global style's paint has no Text to inherit from).
      auto feather = shapePaint->feather();
      renderPaint->feather(feather != nullptr ? feather->strength() : 0.0f);
      if (!owned)
      {
        BlendMode mode = shapePaint->inheritsBlendMode()
                             ? BlendMode::srcOver
                             : shapePaint->blendMode();
        uint8_t amount = shapePaint->additiveAmount();
        renderPaint->blendMode(renderBlendModeFor(mode, amount));
        renderPaint->additiveness(additivenessFor(mode, amount));
      }
      shapePaint->draw(
          renderer,
          path,
          worldTransform,
          true,
          renderPaint,
          true,
          &imageBounds);
    };

    // An effect (dash, trim...) is built from every glyph and replaces
    // whichever bucket's path ShapePaint::draw is handed, so drawing it per
    // bucket would stack the whole outline once per opacity. Draw it once,
    // at the highest opacity any glyph has. The effect's output can't be
    // traced back to glyphs, so fainter glyphs it keeps draw at that opacity
    // too.
    if (shapePaint->lastEffectPath(shapePaint) != nullptr)
    {
      if (opacityPaths.empty())
      {
        continue;
      }
      float opacity = opacityPaths.rbegin()->first;
      if (owned && opacity == 1.0f)
      {
        shapePaint->draw(
            renderer,
            geometry.path(),
            worldTransform,
            true,
            nullptr,
            true,
            &imageBounds);
      }
      else
      {
        geometry.reservePaints(artboard()->factory());
        drawPooled(geometry.path(), opacity, 0);
      }
      continue;
    }

    // For blend modes to work, opaque paths render first
    auto itr = opacityPaths.find(1.0f);
    if (owned && itr != opacityPaths.end())
    {
      ShapePaintPath& path = itr->second;
      shapePaint->draw(
          renderer,
          &path,
          worldTransform,
          true,
          nullptr,
          true,
          &imageBounds);
    }

    geometry.reservePaints(artboard()->factory());

    uint32_t paintIndex = 0;
    for (itr = opacityPaths.begin(); itr != opacityPaths.end(); itr++)
    {
      // Don't render opaque paths twice
      if (owned && itr->first == 1.0f)
      {
        continue;
      }
      drawPooled(&itr->second, itr->first, paintIndex++);
    }
  }
}

ColorInt TextStylePaint::foregroundColor() const
{
  for (auto shapePaint : m_ShapePaints)
  {
    if (shapePaint->is<Fill>() && shapePaint->paint()->is<SolidColor>())
    {
      return (ColorInt)shapePaint->paint()->as<SolidColor>()->colorValue();
    }
  }
  return 0xFF000000;
}

const Mat2D& TextStylePaint::shapeWorldTransform() const
{
  // Global text style paints are parented to the style Artboard; fall back to
  // the artboard's transform (only used for feather inner-path rebuilds).
  if (parent()->is<Text>())
  {
    return parent()->as<Text>()->shapeWorldTransform();
  }
  return artboard()->shapeWorldTransform();
}

Component* TextStylePaint::pathBuilder() { return parent(); }

Core* TextStylePaint::clone() const
{
  TextStylePaint* twin = TextStylePaintBase::clone()->as<TextStylePaint>();
  cloneFontInto(twin);
  return twin;
}
