#include "rive/generated/shapes/paint/target_effect_base.hpp"
#include "rive/shapes/paint/target_effect.hpp"

using namespace rive;

Core* TargetEffectBase::clone() const
{
    auto cloned = new TargetEffect();
    cloned->copy(*this);
    return cloned;
}
