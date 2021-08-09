#include "rive/animation/blend_state_direct_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"

using namespace rive;

BlendStateDirectInstance::BlendStateDirectInstance(
    const BlendStateDirect* blendState) :
    BlendStateInstance<BlendStateDirect, BlendAnimationDirect>(blendState)
{
}

void BlendStateDirectInstance::advance(float seconds, SMIInput** inputs)
{
	BlendStateInstance<BlendStateDirect, BlendAnimationDirect>::advance(seconds,
	                                                                    inputs);
	for (auto& animation : m_AnimationInstances)
	{
		auto inputInstance = inputs[animation.blendAnimation()->inputId()];

		auto numberInput = reinterpret_cast<const SMINumber*>(inputInstance);
		auto value = numberInput->value();
		animation.mix(std::min(1.0f, std::max(0.0f, value / 100.0f)));
	}
}
