#ifndef _RIVE_KEYBOARD_INPUT_HPP_
#define _RIVE_KEYBOARD_INPUT_HPP_
#include "rive/generated/inputs/keyboard_input_base.hpp"
#include "rive/inputs/keyboard_key_phase.hpp"
#include <stdio.h>
namespace rive
{
class KeyboardInput : public KeyboardInputBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif