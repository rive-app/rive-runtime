#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_state_1d_instance.hpp"

using namespace rive;

std::unique_ptr<StateInstance> BlendState1D::makeInstance(
    ArtboardInstance* instance) const
{
    return rivestd::make_unique<BlendState1DInstance>(this, instance);
}
