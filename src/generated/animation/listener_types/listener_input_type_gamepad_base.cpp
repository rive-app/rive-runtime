#include "rive/generated/animation/listener_types/listener_input_type_gamepad_base.hpp"
#include "rive/animation/listener_types/listener_input_type_gamepad.hpp"

using namespace rive;

Core* ListenerInputTypeGamepadBase::clone() const
{
    auto cloned = new ListenerInputTypeGamepad();
    cloned->copy(*this);
    return cloned;
}
