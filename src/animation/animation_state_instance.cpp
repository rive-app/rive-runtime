#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"

using namespace rive;

AnimationStateInstance::AnimationStateInstance(const AnimationState* state,
                                               Artboard* instance) :
    StateInstance(state),
    m_AnimationInstance(state->animation(), instance),
    m_KeepGoing(true)
{}

void AnimationStateInstance::advance(float seconds, SMIInput** inputs) {
    m_KeepGoing = m_AnimationInstance.advance(seconds);
}

void AnimationStateInstance::apply(float mix) {
    m_AnimationInstance.apply(mix);
}

bool AnimationStateInstance::keepGoing() const { return m_KeepGoing; }