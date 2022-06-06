#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"

using namespace rive;

static LinearAnimation emptyAnimation;

AnimationStateInstance::AnimationStateInstance(const AnimationState* state,
                                               ArtboardInstance* instance) :
    StateInstance(state),
    // We're careful to always instance a valid animation here as the
    // StateMachine makes assumptions about AnimationState's producing valid
    // AnimationStateInstances with backing animations. This was discovered when
    // using Clang address sanitizer. We previously returned a
    // SystemStateInstance (basically a no-op StateMachine state) which would
    // cause bad casts in parts of the code where we assumed AnimationStates
    // would have create AnimationStateInstances.
    m_AnimationInstance(state->animation() ? state->animation() : &emptyAnimation, instance),
    m_KeepGoing(true) {}

void AnimationStateInstance::advance(float seconds, Span<SMIInput*>) {
    m_KeepGoing = m_AnimationInstance.advance(seconds);
}

void AnimationStateInstance::apply(float mix) { m_AnimationInstance.apply(mix); }

bool AnimationStateInstance::keepGoing() const { return m_KeepGoing; }