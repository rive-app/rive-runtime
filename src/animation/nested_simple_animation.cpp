#include "rive/animation/nested_simple_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

bool NestedSimpleAnimation::advance(float elapsedSeconds)
{
    bool keepGoing = false;
    if (m_AnimationInstance != nullptr)
    {
        if (isPlaying())
        {
            keepGoing =
                m_AnimationInstance->advance(elapsedSeconds * speed(), m_AnimationInstance.get());
        }
        if (mix() != 0.0f)
        {
            m_AnimationInstance->apply(mix());
        }
    }
    return keepGoing;
}