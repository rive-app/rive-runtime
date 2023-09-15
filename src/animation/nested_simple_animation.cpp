#include "rive/animation/nested_simple_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

void NestedSimpleAnimation::advance(float elapsedSeconds)
{
    if (m_AnimationInstance != nullptr)
    {
        if (isPlaying())
        {
            m_AnimationInstance->advance(elapsedSeconds * speed());
        }
        if (mix() != 0.0f)
        {
            m_AnimationInstance->apply(mix());
        }
    }
}