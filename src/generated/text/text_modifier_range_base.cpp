#include "rive/generated/text/text_modifier_range_base.hpp"
#include "rive/text/text_modifier_range.hpp"

using namespace rive;

Core* TextModifierRangeBase::clone() const
{
    auto cloned = new TextModifierRange();
    cloned->copy(*this);
    return cloned;
}
