#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_EXTRAS_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_EXTRAS_HPP_

// Implementation detail of LinearAnimationInstance. Only
// linear_animation_instance.cpp needs it, and linear_animation_instance.hpp
// forward declares LAIBindingExtras rather than including this, so the hash
// containers below stay out of the public surface.
//
// LinearAnimationInstance is embedded *by value* in AnimationStateInstance and,
// N times over, in a blend state's BlendStateAnimationInstance vector, so every
// inline byte is multiplied by the live state instances in a file. The members
// here are populated only when a file authors a scripted interpolator or a
// data-bound keyframe value. Before adding an inline member there, check
// whether it belongs in here instead.

#include <memory>
#include <unordered_map>
#include <vector>

namespace rive
{
class BindableProperty;
class DataBind;
class InterpolatingKeyFrame;
class KeyFrame;
class StatefulScriptedInterpolator;

/// The cold cluster of LinearAnimationInstance. Allocated the first time this
/// instance vends a stateful scripted interpolator or a data-bound keyframe
/// value; never allocated otherwise.
///
/// Teardown of these is order sensitive — see the comment in
/// ~LinearAnimationInstance, which does it explicitly rather than leaning on
/// declaration order.
struct LAIBindingExtras
{
  /// Per-(this LAI, keyframe) stateful clones of the shared
  /// ScriptedInterpolator templates. Each holder pulls the input binds it
  /// parked on the artboard before destroying its clone (and Lua ref).
  std::unordered_map<
      const InterpolatingKeyFrame*,
      std::unique_ptr<StatefulScriptedInterpolator>>
      scriptedInterpolators;

  /// Per-keyframe holders receiving data-bound values for this instance,
  /// keyed by the shared KeyFrame*. Built lazily by keyFrameValueHolder() on
  /// first apply of a bound keyframe; owned here.
  std::unordered_map<const KeyFrame*, BindableProperty*> keyFrameValueHolders;

  /// Clones of the source keyframe data binds, retargeted to the holders
  /// above and appended to the artboard's data-bind container, keyed by the
  /// keyframe so keyFrameValueHolder can refresh a holder at read time.
  /// Removed + deleted BEFORE the holders they target, same discipline as
  /// the input binds of `scriptedInterpolators`.
  std::unordered_map<const KeyFrame*, DataBind*> keyFrameValueBinds;
};
} // namespace rive
#endif
