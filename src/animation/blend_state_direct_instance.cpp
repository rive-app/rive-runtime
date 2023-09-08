
#include "rive/animation/blend_animation_direct.hpp"
#include "rive/animation/blend_state_direct_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include <iostream>

using namespace rive;

BlendStateDirectInstance::BlendStateDirectInstance(const BlendStateDirect* blendState,
                                                   ArtboardInstance* instance) :
    BlendStateInstance<BlendStateDirect, BlendAnimationDirect>(blendState, instance)
{}

void BlendStateDirectInstance::advance(float seconds, StateMachineInstance* stateMachineInstance)
{
    BlendStateInstance<BlendStateDirect, BlendAnimationDirect>::advance(seconds,
                                                                        stateMachineInstance);

    for (auto& animation : m_AnimationInstances)
    {
        if (animation.blendAnimation()->blendSource() ==
            static_cast<int>(DirectBlendSource::mixValue))
        {
            auto value = animation.blendAnimation()->mixValue();
            animation.mix(std::min(1.0f, std::max(0.0f, value / 100.0f)));
        }
        else
        {
            auto inputInstance = stateMachineInstance->input(animation.blendAnimation()->inputId());
            auto numberInput = static_cast<const SMINumber*>(inputInstance);
            auto value = numberInput->value();
            animation.mix(std::min(1.0f, std::max(0.0f, value / 100.0f)));
        }
    }
}
