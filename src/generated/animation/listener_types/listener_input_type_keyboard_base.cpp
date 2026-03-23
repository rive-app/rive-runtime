#include "rive/generated/animation/listener_types/listener_input_type_keyboard_base.hpp"
#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"

using namespace rive;

Core* ListenerInputTypeKeyboardBase::clone() const
{
    auto cloned = new ListenerInputTypeKeyboard();
    cloned->copy(*this);
    return cloned;
}
