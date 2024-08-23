#ifndef _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#define _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#include "rive/generated/animation/nested_linear_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class LinearAnimationInstance;
class NestedLinearAnimation : public NestedLinearAnimationBase
{
protected:
    std::unique_ptr<LinearAnimationInstance> m_AnimationInstance;

public:
    NestedLinearAnimation();
    ~NestedLinearAnimation() override;

    void initializeAnimation(ArtboardInstance*) override;
    LinearAnimationInstance* animationInstance() { return m_AnimationInstance.get(); }
};
} // namespace rive

#endif