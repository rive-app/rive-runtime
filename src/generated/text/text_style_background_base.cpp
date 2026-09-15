#include "rive/generated/text/text_style_background_base.hpp"
#include "rive/text/text_style_background.hpp"

using namespace rive;

Core* TextStyleBackgroundBase::clone() const
{
    auto cloned = new TextStyleBackground();
    cloned->copy(*this);
    return cloned;
}
