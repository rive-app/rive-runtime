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

bool TextStylePaint::addPath(const RawPath& rawPath, float opacity)
{
  return m_geometry.addPath(rawPath, opacity);
}

void TextStylePaint::draw(Renderer* renderer, const Mat2D& worldTransform)
{
  drawGeometry(renderer, worldTransform, m_geometry, 1.0f);
}

void TextStylePaint::drawGeometry(
    Renderer* renderer,
    const Mat2D& worldTransform,
    TextStyleGeometry& geometry,
    float opacityModifier)
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
    // A global text style's paint lives under the style Artboard, not a
    // Text; keep its shape paint's own (default) blend mode in that case.
    if (owned && parent()->is<Text>())
    {
      auto text = parent()->as<Text>();
      shapePaint->blendMode(text->blendMode(), text->additiveAmount());
    }

    // For blend modes to work, opaque paths render first
    auto itr = opacityPaths.find(1.0f);
    if (owned && itr != opacityPaths.end())
    {
      ShapePaintPath& path = itr->second;
      shapePaint->draw(renderer, &path, worldTransform, true);
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
      RenderPaint* renderPaint = geometry.pooledPaint(paintIndex++);
      shapePaint->applyTo(renderPaint, itr->first * opacityModifier);
      // Pooled paints are shared across shape paints; always set feather
      // and blend mode so one paint's settings don't leak into the
      // others. Pooled draws take the shape paint's own mode (a global
      // style's paint has no Text to inherit from).
      auto feather = shapePaint->feather();
      renderPaint->feather(feather != nullptr ? feather->strength() : 0.0f);
      if (!owned)
      {
        BlendMode mode = shapePaint->inheritsBlendMode()
                             ? BlendMode::srcOver
                             : shapePaint->blendMode();
        renderPaint->blendMode(mode);
        renderPaint->additiveness(
            additivenessFor(mode, shapePaint->additiveAmount()));
      }
      ShapePaintPath& path = itr->second;
      shapePaint->draw(renderer, &path, worldTransform, true, renderPaint);
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
  if (m_fileAsset != nullptr)
  {
    twin->setAsset(m_fileAsset);
  }

  return twin;
}
