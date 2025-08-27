#include "rive/generated/animation/listener_number_change_base.hpp"
#include "rive/animation/listener_number_change.hpp"

using namespace rive;

Core* ListenerNumberChangeBase::clone() const
{
    auto cloned = new ListenerNumberChange();
    cloned->copy(*this);
    return cloned;
}
