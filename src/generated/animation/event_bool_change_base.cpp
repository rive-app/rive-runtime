#include "rive/generated/animation/event_bool_change_base.hpp"
#include "rive/animation/event_bool_change.hpp"

using namespace rive;

Core* EventBoolChangeBase::clone() const {
    auto cloned = new EventBoolChange();
    cloned->copy(*this);
    return cloned;
}
