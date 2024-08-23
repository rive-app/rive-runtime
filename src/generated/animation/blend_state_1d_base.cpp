#include "rive/generated/animation/blend_state_1d_base.hpp"
#include "rive/animation/blend_state_1d.hpp"

using namespace rive;

Core* BlendState1DBase::clone() const
{
    auto cloned = new BlendState1D();
    cloned->copy(*this);
    return cloned;
}
