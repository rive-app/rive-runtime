#include "rive/generated/animation/event_number_change_base.hpp"
#include "rive/animation/event_number_change.hpp"

using namespace rive;

Core* EventNumberChangeBase::clone() const {
    auto cloned = new EventNumberChange();
    cloned->copy(*this);
    return cloned;
}
