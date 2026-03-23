#include "rive/generated/inputs/keyboard_input_base.hpp"
#include "rive/inputs/keyboard_input.hpp"

using namespace rive;

Core* KeyboardInputBase::clone() const
{
    auto cloned = new KeyboardInput();
    cloned->copy(*this);
    return cloned;
}
