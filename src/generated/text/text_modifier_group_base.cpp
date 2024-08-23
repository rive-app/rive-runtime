#include "rive/generated/text/text_modifier_group_base.hpp"
#include "rive/text/text_modifier_group.hpp"

using namespace rive;

Core* TextModifierGroupBase::clone() const
{
    auto cloned = new TextModifierGroup();
    cloned->copy(*this);
    return cloned;
}
