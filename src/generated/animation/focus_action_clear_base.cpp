#include "rive/generated/animation/focus_action_clear_base.hpp"
#include "rive/animation/focus_action_clear.hpp"

using namespace rive;

Core* FocusActionClearBase::clone() const
{
    auto cloned = new FocusActionClear();
    cloned->copy(*this);
    return cloned;
}
