#ifndef _RIVE_NESTED_SIMPLE_ANIMATION_HPP_
#define _RIVE_NESTED_SIMPLE_ANIMATION_HPP_
#include "rive/generated/animation/nested_simple_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedSimpleAnimation : public NestedSimpleAnimationBase
{
public:
    bool advance(float elapsedSeconds, bool newFrame) override;
};
} // namespace rive

#endif