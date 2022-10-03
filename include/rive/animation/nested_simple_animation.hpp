#ifndef _RIVE_NESTED_SIMPLE_ANIMATION_HPP_
#define _RIVE_NESTED_SIMPLE_ANIMATION_HPP_
#include "rive/generated/animation/nested_simple_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedSimpleAnimation : public NestedSimpleAnimationBase
{
public:
    void advance(float elapsedSeconds) override;
};
} // namespace rive

#endif