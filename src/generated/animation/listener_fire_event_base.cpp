#include "rive/generated/animation/listener_fire_event_base.hpp"
#include "rive/animation/listener_fire_event.hpp"

using namespace rive;

Core* ListenerFireEventBase::clone() const
{
    auto cloned = new ListenerFireEvent();
    cloned->copy(*this);
    return cloned;
}
