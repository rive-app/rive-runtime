#include "rive/event.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

void Event::trigger(const CallbackData& value)
{
    value.context()->reportEvent(this, value.delaySeconds());
}