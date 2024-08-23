#include "rive/animation/animation_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/animation_state_instance.hpp"
#include "rive/core_context.hpp"
#include "rive/artboard.hpp"

using namespace rive;

std::unique_ptr<StateInstance> AnimationState::makeInstance(ArtboardInstance* instance) const
{
    return rivestd::make_unique<AnimationStateInstance>(this, instance);
}

#ifdef TESTING
void AnimationState::animation(LinearAnimation* animation) { m_Animation = animation; }
#endif
