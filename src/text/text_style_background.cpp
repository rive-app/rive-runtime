#include "rive/text/text_style_background.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text.hpp"
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

void TextStyleBackground::resetPath()
{
    m_rects.clear();
    m_path.rewind();
}

void TextStyleBackground::addRect(const AABB& rect) { m_rects.push_back(rect); }

void TextStyleBackground::updatePath()
{
    m_path.update(m_rects, cornerRadius());
}

void TextStyleBackground::draw(Renderer* renderer, const Mat2D& worldTransform)
{
    if (m_rects.empty())
    {
        return;
    }
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->shouldDraw())
        {
            continue;
        }
        shapePaint->blendMode(style()->parent()->as<Text>()->blendMode());
        shapePaint->draw(renderer, &m_path, worldTransform, true);
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
