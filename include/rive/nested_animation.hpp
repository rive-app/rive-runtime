#ifndef _RIVE_NESTED_ANIMATION_HPP_
#define _RIVE_NESTED_ANIMATION_HPP_
#include "rive/generated/nested_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardInstance;

class NestedAnimation : public NestedAnimationBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;

    // Advance animations and apply them to the artboard.
    virtual void advance(float elapsedSeconds) = 0;

    // Initialize the animation (make instances as necessary) from the
    // source artboard.
    virtual void initializeAnimation(ArtboardInstance*) = 0;
};
} // namespace rive

#endif