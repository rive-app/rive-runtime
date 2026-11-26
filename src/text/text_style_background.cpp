#include "rive/text/text_style_background.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

StatusCode TextStyleBackground::onAddedDirty(CoreContext* context)
{
  StatusCode code = Super::onAddedDirty(context);
  if (code == StatusCode::Ok)
  {
    if (!parent()->is<TextStylePaint>())
    {
      return StatusCode::InvalidObject;
    }
    parent()->as<TextStylePaint>()->background(this);
  }
  return code;
}

TextStylePaint* TextStyleBackground::style() const
{
  return parent()->as<TextStylePaint>();
}

void TextStyleBackground::resetPath() { m_adornment.reset(); }

void TextStyleBackground::addRect(const AABB& rect)
{
  m_adornment.addRect(rect);
}

void TextStyleBackground::buildDependencies()
{
  Super::buildDependencies();
  // Nothing adds us to the dependency graph otherwise, and a Feather under
  // one of our paints hangs off us -- without this it would never update.
  // Depending on the TextStylePaint (which itself depends on the Text) also
  // guarantees we sort after the text has laid out and filled our rects.
  if (parent() != nullptr)
  {
    parent()->addDependent(this);
  }
}

void TextStyleBackground::updatePath()
{
  m_adornment.update(cornerRadius());
  // Dashes and trims cache what they made of the old path.
  invalidateStrokeEffects();
  // Text rebuilds this path from its own update pass; the feathers derive
  // their geometry from it and sort after us, so dirty them to run again.
  for (auto shapePaint : m_ShapePaints)
  {
    auto feather = shapePaint->feather();
    if (feather != nullptr)
    {
      feather->addDirt(ComponentDirt::Path);
    }
  }
}

void TextStyleBackground::draw(
    Renderer* renderer,
    const Mat2D& worldTransform,
    const AABB* imageBounds)
{
  if (m_adornment.empty())
  {
    return;
  }
  // A global text style's background lives under the style artboard, not a
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

const Mat2D& TextStyleBackground::shapeWorldTransform() const
{
  return style()->shapeWorldTransform();
}

Component* TextStyleBackground::pathBuilder() { return style()->pathBuilder(); }

void TextStyleBackground::cornerRadiusChanged()
{
  if (parent() == nullptr || parent()->parent() == nullptr ||
      !parent()->parent()->is<Text>())
  {
    return;
  }
  parent()->parent()->as<Text>()->markPaintDirty();
}
