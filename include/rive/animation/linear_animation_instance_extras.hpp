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
class ScriptedInterpolator;

/// The cold cluster of LinearAnimationInstance. Allocated the first time this
/// instance vends a stateful scripted interpolator or a data-bound keyframe
/// value; never allocated otherwise.
///
/// Teardown of these four is order sensitive — see the comment in
/// ~LinearAnimationInstance, which does it explicitly rather than leaning on
/// declaration order.
struct LAIBindingExtras
{
    /// Per-(this LAI, keyframe) stateful clones of the shared
    /// ScriptedInterpolator templates. The unique_ptr destroys the clone (and
    /// its Lua ref) with this cluster.
    std::unordered_map<const InterpolatingKeyFrame*,
                       std::unique_ptr<ScriptedInterpolator>>
        scriptedInterpolators;

    /// Data binds that cloneProperties() appended to the artboard on our
    /// behalf for the clones above. ~LinearAnimationInstance must
    /// removeDataBind + delete each of these BEFORE `scriptedInterpolators`
    /// tears down, because the bind targets point at CustomPropertys owned by
    /// the clones.
    std::vector<DataBind*> clonedArtboardDataBinds;

    /// Per-keyframe holders receiving data-bound values for this instance,
    /// keyed by the shared KeyFrame*. Built lazily by keyFrameValueHolder() on
    /// first apply of a bound keyframe; owned here.
    std::unordered_map<const KeyFrame*, BindableProperty*> keyFrameValueHolders;

    /// Clones of the source keyframe data binds, retargeted to the holders
    /// above and appended to the artboard's data-bind container, keyed by the
    /// keyframe so keyFrameValueHolder can refresh a holder at read time.
    /// Removed + deleted BEFORE the holders they target, same discipline as
    /// `clonedArtboardDataBinds`.
    std::unordered_map<const KeyFrame*, DataBind*> keyFrameValueBinds;
};
} // namespace rive
#endif
