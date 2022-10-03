#ifndef _RIVE_BLEND_ANIMATION1_D_HPP_
#define _RIVE_BLEND_ANIMATION1_D_HPP_
#include "rive/generated/animation/blend_animation_1d_base.hpp"
#include <stdio.h>
namespace rive
{
class BlendAnimation1D : public BlendAnimation1DBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif