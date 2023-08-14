#include "rive/generated/event_base.hpp"
#include "rive/event.hpp"

using namespace rive;

Core* EventBase::clone() const
{
    auto cloned = new Event();
    cloned->copy(*this);
    return cloned;
}
