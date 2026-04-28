#include "rive/generated/animation/focus_action_target_base.hpp"
#include "rive/animation/focus_action_target.hpp"

using namespace rive;

Core* FocusActionTargetBase::clone() const
{
    auto cloned = new FocusActionTarget();
    cloned->copy(*this);
    return cloned;
}
