#ifndef _RIVE_NESTED_REMAP_ANIMATION_HPP_
#define _RIVE_NESTED_REMAP_ANIMATION_HPP_
#include "rive/generated/animation/nested_remap_animation_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedRemapAnimation : public NestedRemapAnimationBase
{
public:
  void timeChanged() override;
  bool advance(float elapsedSeconds, bool newFrame) override;
  void initializeAnimation(ArtboardInstance*) override;

  // The time changed since the animation was last applied. A host's settle
  // passes (advances that are not a new frame) apply it then, so a time
  // written after this frame's advance (a data bind, a joystick, or a host
  // animation applied late) shows this frame instead of the next (see
  // NestedArtboard::advanceComponent for when they can't).
  bool needsApply() const { return m_needsApply; }

private:
  // Sits in the base's tail padding: no size change.
  bool m_needsApply = true;
};
} // namespace rive

#endif