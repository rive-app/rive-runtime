#include "rive/generated/text/text_style_paint_base.hpp"
#include "rive/text/text_style_paint.hpp"

using namespace rive;

Core* TextStylePaintBase::clone() const
{
    auto cloned = new TextStylePaint();
    cloned->copy(*this);
    return cloned;
}
