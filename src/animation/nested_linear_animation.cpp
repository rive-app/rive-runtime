#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

NestedLinearAnimation::NestedLinearAnimation() {}
NestedLinearAnimation::~NestedLinearAnimation() {}

void NestedLinearAnimation::initializeAnimation(ArtboardInstance* artboard)
{
    m_AnimationInstance = std::make_unique<LinearAnimationInstance>(
        artboard->animation(animationId()),
        artboard);
}

void NestedLinearAnimation::releaseDependencies()
{
    // m_AnimationInstance holds m_artboardInstance = the NestedArtboard's
    // mounted m_Instance, and ~NestedArtboard frees that instance immediately
    // after calling this. ~LinearAnimationInstance dereferences the pointer
    // (removeDataBind for scripted-interpolator and data-bound-keyframe
    // clones), so the instance has to die here rather than later, when the
    // parent artboard deletes us from its m_Objects. Mirrors
    // NestedStateMachine::releaseDependencies.
    m_AnimationInstance.reset(nullptr);
}
