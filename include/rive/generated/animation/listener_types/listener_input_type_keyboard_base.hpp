#ifndef _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
namespace rive
{
class ListenerInputTypeKeyboardBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 665;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif