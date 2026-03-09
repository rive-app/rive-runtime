#include "rive/generated/animation/focus_action_base.hpp"
#include "rive/animation/focus_action.hpp"

using namespace rive;

Core* FocusActionBase::clone() const
{
    auto cloned = new FocusAction();
    cloned->copy(*this);
    return cloned;
}
