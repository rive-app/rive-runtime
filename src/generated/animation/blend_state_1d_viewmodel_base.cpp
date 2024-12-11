#include "rive/generated/animation/blend_state_1d_viewmodel_base.hpp"
#include "rive/animation/blend_state_1d_viewmodel.hpp"

using namespace rive;

Core* BlendState1DViewModelBase::clone() const
{
    auto cloned = new BlendState1DViewModel();
    cloned->copy(*this);
    return cloned;
}
