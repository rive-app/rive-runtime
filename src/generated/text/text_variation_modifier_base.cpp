#include "rive/generated/text/text_variation_modifier_base.hpp"
#include "rive/text/text_variation_modifier.hpp"

using namespace rive;

Core* TextVariationModifierBase::clone() const
{
    auto cloned = new TextVariationModifier();
    cloned->copy(*this);
    return cloned;
}
