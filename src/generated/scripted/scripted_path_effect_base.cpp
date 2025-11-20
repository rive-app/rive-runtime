#include "rive/generated/scripted/scripted_path_effect_base.hpp"
#include "rive/scripted/scripted_path_effect.hpp"

using namespace rive;

Core* ScriptedPathEffectBase::clone() const
{
    auto cloned = new ScriptedPathEffect();
    cloned->copy(*this);
    return cloned;
}
