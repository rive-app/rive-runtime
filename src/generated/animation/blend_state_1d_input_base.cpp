#include "rive/generated/animation/blend_state_1d_input_base.hpp"
#include "rive/animation/blend_state_1d_input.hpp"

using namespace rive;

Core* BlendState1DInputBase::clone() const
{
    auto cloned = new BlendState1DInput();
    cloned->copy(*this);
    return cloned;
}
