#include "rive/generated/animation/listener_types/listener_input_type_pointer_button_base.hpp"
#include "rive/animation/listener_types/listener_input_type_pointer_button.hpp"

using namespace rive;

Core* ListenerInputTypePointerButtonBase::clone() const
{
    auto cloned = new ListenerInputTypePointerButton();
    cloned->copy(*this);
    return cloned;
}
