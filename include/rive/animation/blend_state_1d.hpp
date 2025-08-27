#ifndef _RIVE_BLEND_STATE1_D_HPP_
#define _RIVE_BLEND_STATE1_D_HPP_
#include "rive/generated/animation/blend_state_1d_base.hpp"

namespace rive
{
class BlendState1D : public BlendState1DBase
{
public:
    std::unique_ptr<StateInstance> makeInstance(
        ArtboardInstance*) const override;
};
} // namespace rive

#endif