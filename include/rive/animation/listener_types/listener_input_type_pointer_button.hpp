#ifndef _RIVE_LISTENER_INPUT_TYPE_POINTER_BUTTON_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_POINTER_BUTTON_HPP_
#include "rive/generated/animation/listener_types/listener_input_type_pointer_button_base.hpp"
namespace rive
{
class ListenerInputTypePointerButton : public ListenerInputTypePointerButtonBase
{
public:
    PointerButton pointerButton() const override
    {
        return (PointerButton)pointerButtonValue();
    }
};
} // namespace rive

#endif
