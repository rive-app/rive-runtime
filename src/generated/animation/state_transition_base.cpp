#include "rive/generated/animation/state_transition_base.hpp"
#include "rive/animation/state_transition.hpp"

using namespace rive;

Core* StateTransitionBase::clone() const
{
    auto cloned = new StateTransition();
    cloned->copy(*this);
    return cloned;
}
