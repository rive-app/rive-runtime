#include "rive/generated/animation/listener_trigger_change_base.hpp"
#include "rive/animation/listener_trigger_change.hpp"

using namespace rive;

Core* ListenerTriggerChangeBase::clone() const
{
    auto cloned = new ListenerTriggerChange();
    cloned->copy(*this);
    return cloned;
}
