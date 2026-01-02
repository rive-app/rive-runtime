#include "rive/generated/shapes/paint/group_effect_base.hpp"
#include "rive/shapes/paint/group_effect.hpp"

using namespace rive;

Core* GroupEffectBase::clone() const
{
    auto cloned = new GroupEffect();
    cloned->copy(*this);
    return cloned;
}
