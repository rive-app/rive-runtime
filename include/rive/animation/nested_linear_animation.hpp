#ifndef _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#define _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#include "rive/generated/animation/nested_linear_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class LinearAnimation;
class LinearAnimationInstance;
class NestedLinearAnimation : public NestedLinearAnimationBase
{
protected:
  std::unique_ptr<LinearAnimationInstance> m_AnimationInstance;

public:
  NestedLinearAnimation();
  ~NestedLinearAnimation() override;

  void initializeAnimation(ArtboardInstance*) override;
  // Editor hosts resolve the definition by persistent ID, while the target
  // remains the particular playback instance receiving animation writes.
  void initializeAnimation(ArtboardInstance*, const LinearAnimation*);
  // Defined in the .cpp: LinearAnimationInstance is only forward declared
  // here, so unique_ptr::reset cannot be instantiated inline.
  void releaseDependencies() override;
  LinearAnimationInstance* animationInstance()
  {
    return m_AnimationInstance.get();
  }
};
} // namespace rive

#endif