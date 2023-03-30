#ifndef _RIVE_BLEND_ANIMATION_DIRECT_HPP_
#define _RIVE_BLEND_ANIMATION_DIRECT_HPP_
#include "rive/generated/animation/blend_animation_direct_base.hpp"
#include <stdio.h>
namespace rive
{

enum class DirectBlendSource : unsigned int
{
    inputId = 0,
    mixValue = 1,
};

class BlendAnimationDirect : public BlendAnimationDirectBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif