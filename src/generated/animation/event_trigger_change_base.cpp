#include "rive/generated/animation/event_trigger_change_base.hpp"
#include "rive/animation/event_trigger_change.hpp"

using namespace rive;

Core* EventTriggerChangeBase::clone() const {
    auto cloned = new EventTriggerChange();
    cloned->copy(*this);
    return cloned;
}
