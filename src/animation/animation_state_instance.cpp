#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/blend_accumulator.hpp"

using namespace rive;

static LinearAnimation emptyAnimation;

AnimationStateInstance::AnimationStateInstance(
    const AnimationState* state,
    ArtboardInstance* instance)
    : StateInstance(state)
    ,
    // We're careful to always instance a valid animation here as the
    // StateMachine makes assumptions about AnimationState's producing valid
    // AnimationStateInstances with backing animations. This was discovered when
    // using Clang address sanitizer. We previously returned a
    // SystemStateInstance (basically a no-op StateMachine state) which would
    // cause bad casts in parts of the code where we assumed AnimationStates
    // would have create AnimationStateInstances.
    m_AnimationInstance(
        state->animation() ? state->animation() : &emptyAnimation,
        instance,
        state->speed())
    , m_KeepGoing(true)
{}

// NOTE:: should we return bool here? we are not currently using the output of
// this, we are instead using m_keepGoing directly.
void AnimationStateInstance::advance(
    float seconds,
    StateMachineInstance* stateMachineInstance)
{
  m_KeepGoing = m_AnimationInstance.advance(
      seconds * state()->as<AnimationState>()->speed(),
      stateMachineInstance);
}

void AnimationStateInstance::apply(ArtboardInstance* instance, float mix)
{
  m_AnimationInstance.apply(mix);
}

bool AnimationStateInstance::keepGoing() const { return m_KeepGoing; }
void AnimationStateInstance::clearSpilledTime()
{
  m_AnimationInstance.clearSpilledTime();
}

bool AnimationStateInstance::restart()
{
  auto animationState = state()->as<AnimationState>();
  // The state may have been given another animation since this instance was
  // made (an editor reassigning it): a new instance plays that one.
  auto animation = animationState->animation() ? animationState->animation()
                                               : &emptyAnimation;
  if (m_AnimationInstance.animation() != animation ||
      !m_AnimationInstance.restart(animationState->speed()))
  {
    return false;
  }
  m_KeepGoing = true;
  return true;
}

void AnimationStateInstance::layerAccumulator(BlendAccumulator* accumulator)
{
  // An animation that keys nothing the machine tracks never asks it.
  m_AnimationInstance.blendAccumulator(
      accumulator != nullptr &&
              accumulator->tracksAny(m_AnimationInstance.animation())
          ? accumulator
          : nullptr);
}
