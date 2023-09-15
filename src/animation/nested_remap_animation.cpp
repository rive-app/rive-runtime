#include "rive/animation/nested_remap_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

void NestedRemapAnimation::timeChanged()
{
    if (m_AnimationInstance != nullptr)
    {
        m_AnimationInstance->time(m_AnimationInstance->animation()->globalToLocalSeconds(
            m_AnimationInstance->durationSeconds() * time()));
    }
}

void NestedRemapAnimation::initializeAnimation(ArtboardInstance* artboard)
{
    Super::initializeAnimation(artboard);
    timeChanged();
}

void NestedRemapAnimation::advance(float elapsedSeconds)
{
    if (m_AnimationInstance != nullptr && mix() != 0.0f)
    {
        m_AnimationInstance->apply(mix());
    }
}