#include "rive/generated/animation/blend_state_direct_base.hpp"
#include "rive/animation/blend_state_direct.hpp"

using namespace rive;

Core* BlendStateDirectBase::clone() const
{
    auto cloned = new BlendStateDirect();
    cloned->copy(*this);
    return cloned;
}
