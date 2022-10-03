#ifndef _RIVE_BLEND_STATE1_D_HPP_
#define _RIVE_BLEND_STATE1_D_HPP_
#include "rive/generated/animation/blend_state_1d_base.hpp"

namespace rive
{
class BlendState1D : public BlendState1DBase
{
public:
    //  -1 (4294967295) is our flag value for input not set. It means it wasn't set at edit
    //  time.
    const uint32_t noInputSpecified = -1;
    bool hasValidInputId() const { return inputId() != noInputSpecified; }

    StatusCode import(ImportStack& importStack) override;

    std::unique_ptr<StateInstance> makeInstance(ArtboardInstance*) const override;
};
} // namespace rive

#endif