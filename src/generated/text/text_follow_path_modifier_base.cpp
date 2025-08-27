#include "rive/generated/text/text_follow_path_modifier_base.hpp"
#include "rive/text/text_follow_path_modifier.hpp"

using namespace rive;

Core* TextFollowPathModifierBase::clone() const
{
    auto cloned = new TextFollowPathModifier();
    cloned->copy(*this);
    return cloned;
}
