#include "rive/generated/inputs/gamepad_input_base.hpp"
#include "rive/inputs/gamepad_input.hpp"

using namespace rive;

Core* GamepadInputBase::clone() const
{
    auto cloned = new GamepadInput();
    cloned->copy(*this);
    return cloned;
}
