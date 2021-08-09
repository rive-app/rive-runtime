#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"

using namespace rive;

AnimationStateInstance::AnimationStateInstance(const AnimationState* state) :
    StateInstance(state),
    m_AnimationInstance(state->animation()),
    m_KeepGoing(true)
{
}

void AnimationStateInstance::advance(float seconds, SMIInput** inputs)
{
	m_KeepGoing = m_AnimationInstance.advance(seconds);
}

void AnimationStateInstance::apply(Artboard* artboard, float mix)
{
	m_AnimationInstance.apply(artboard, mix);
}

bool AnimationStateInstance::keepGoing() const { return m_KeepGoing; }