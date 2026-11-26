#include "rive/text/text_style_decoration.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/text_engine.hpp"

using namespace rive;

StatusCode TextStyleDecoration::onAddedDirty(CoreContext* context)
{
  StatusCode code = Super::onAddedDirty(context);
#ifndef WITH_RIVE_EDITOR
  // Runtime-only; editor build registers via editorParentChanged.
  if (code == StatusCode::Ok)
  {
    if (!parent()->is<TextStylePaint>())
    {
      return StatusCode::InvalidObject;
    }
    parent()->as<TextStylePaint>()->addDecoration(this);
  }
#endif
  return code;
}

TextStylePaint* TextStyleDecoration::style() const
{
  return parent()->as<TextStylePaint>();
}

void TextStyleDecoration::buildDependencies()
{
  Super::buildDependencies();
  // Like TextStyleBackground: nothing else puts us in the dependency graph,
  // and a Feather under one of our paints hangs off us. Depending on the
  // TextStylePaint (which depends on the Text) sorts us after the layout
  // that fills our rects.
  if (parent() != nullptr)
  {
    parent()->addDependent(this);
  }
}

void TextStyleDecoration::resetPath() { m_adornment.reset(); }

void TextStyleDecoration::addGlyph(
    float left,
    float right,
    float baseline,
    const Font* font,
    float fontSize,
    const Mat2D* transform,
    float opacity)
{
  if (font == nullptr)
  {
    return;
  }
  bool strike = decorationType() == TextDecorationType::strikethrough;
  float position = strike ? font->strikeoutPosition(fontSize)
                          : font->underlinePosition(fontSize);
  float height = thickness();
  if (height < 0.0f)
  {
    height = strike ? font->strikeoutThickness(fontSize)
                    : font->underlineThickness(fontSize);
  }
  if (height <= 0.0f || right <= left)
  {
    return;
  }
  float top = baseline + position + offset();
  AABB rect(left, top, right, top + height);
  if (transform == nullptr && opacity == 1.0f)
  {
    m_adornment.addRect(rect);
  }
  else
  {
    m_adornment.addModifiedRect(rect, transform, opacity);
  }
}

void TextStyleDecoration::updatePath()
{
  m_adornment.update(0.0f);
  // Dashes and trims cache what they made of the old path.
  invalidateStrokeEffects();
  // Text rebuilds this path from its own update pass; feathers derive their
  // geometry from it and sort after us, so dirty them to run again.
  for (auto shapePaint : m_ShapePaints)
  {
    auto feather = shapePaint->feather();
    if (feather != nullptr)
    {
      feather->addDirt(ComponentDirt::Path);
    }
  }
}

void TextStyleDecoration::draw(
    Renderer* renderer,
    const Mat2D& worldTransform,
    const AABB* imageBounds)
{
  if (m_adornment.empty())
  {
    return;
  }
  // A global text style's decoration lives under the style artboard, not a
  // Text; keep its paint's own blend mode then.
  Text* text = style()->parent()->is<Text>() ? style()->parent()->as<Text>()
                                             : nullptr;
  for (auto shapePaint : m_ShapePaints)
  {
    if (!shapePaint->shouldDraw())
    {
      continue;
    }
    if (text != nullptr)
    {
      shapePaint->blendMode(text->blendMode(), text->additiveAmount());
    }
    m_adornment.draw(
        renderer,
        shapePaint,
        worldTransform,
        artboard()->factory(),
        text != nullptr ? text->blendMode() : BlendMode::srcOver,
        text != nullptr ? text->additiveAmount() : 0,
        imageBounds);
  }
}

const Mat2D& TextStyleDecoration::shapeWorldTransform() const
{
  return style()->shapeWorldTransform();
}

Component* TextStyleDecoration::pathBuilder() { return style()->pathBuilder(); }

void TextStyleDecoration::markStyleDirty(TextStylePaint* style)
{
  if (style == nullptr)
  {
    return;
  }
  if (style->parent() != nullptr && style->parent()->is<Text>())
  {
    style->parent()->as<Text>()->markPaintDirty();
  }
  else if (style->owner() != nullptr)
  {
    // A global text style's paint sits under its style artboard, so nothing
    // else tells the Texts drawing it (see TextStyle::metricsChanged).
    style->owner()->styleShapeChanged();
  }
}

void TextStyleDecoration::markTextPaintDirty()
{
  if (parent() != nullptr && parent()->is<TextStylePaint>())
  {
    markStyleDirty(parent()->as<TextStylePaint>());
  }
}

void TextStyleDecoration::decorationTypeValueChanged() { markTextPaintDirty(); }

void TextStyleDecoration::thicknessChanged() { markTextPaintDirty(); }

void TextStyleDecoration::offsetChanged() { markTextPaintDirty(); }
