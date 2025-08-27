#ifndef _RIVE_BLEND_STATE1_DINPUT_HPP_
#define _RIVE_BLEND_STATE1_DINPUT_HPP_
#include "rive/generated/animation/blend_state_1d_input_base.hpp"
#include <stdio.h>
namespace rive
{
class BlendState1DInput : public BlendState1DInputBase
{
public:
    bool hasValidInputId() const { return inputId() != Core::emptyId; }

    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif