#include "rive/generated/animation/listener_types/listener_input_type_base.hpp"
#include "rive/animation/listener_types/listener_input_type.hpp"

using namespace rive;

Core* ListenerInputTypeBase::clone() const
{
    auto cloned = new ListenerInputType();
    cloned->copy(*this);
    return cloned;
}
