#ifndef _RIVE_GAMEPAD_INPUT_HPP_
#define _RIVE_GAMEPAD_INPUT_HPP_
#include "rive/generated/inputs/gamepad_input_base.hpp"
#include "rive/inputs/gamepad_button_phase.hpp"
#include "rive/inputs/gamepad_input_kind.hpp"
namespace rive
{
class GamepadInput : public GamepadInputBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif
