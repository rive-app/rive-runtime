#include "rive/animation/animation_reset_factory.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/animation_state.hpp"

using namespace rive;

std::unique_ptr<AnimationReset> AnimationResetFactory::getInstance()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  if (m_resources.size() > 0)
  {
    auto instance = std::move(m_resources.back());
    m_resources.pop_back();
    return instance;
  }
  auto instance = std::make_unique<AnimationReset>();
  return instance;
}

// The animation an animation state plays, or null.
static const LinearAnimation* stateAnimation(StateInstance* stateInstance)
{
  if (stateInstance == nullptr)
  {
    return nullptr;
  }
  auto state = stateInstance->state();
  return state->is<AnimationState>() ? state->as<AnimationState>()->animation()
                                     : nullptr;
}

std::unique_ptr<AnimationReset> AnimationResetFactory::fromStates(
    StateInstance* stateFrom,
    StateInstance* currentState,
    ArtboardInstance* artboard)
{
  const LinearAnimation* animations[2];
  size_t count = 0;
  for (auto state : {stateFrom, currentState})
  {
    if (auto animation = stateAnimation(state))
    {
      animations[count++] = animation;
    }
  }
  auto animationReset = getInstance();
  animationReset->build(animations, count, artboard, false);
  return animationReset;
}

std::unique_ptr<AnimationReset> AnimationResetFactory::fromAnimations(
    std::vector<const LinearAnimation*>& animations,
    ArtboardInstance* artboard,
    bool useFirstAsBaseline)
{
  auto animationReset = getInstance();
  animationReset->build(
      animations.data(),
      animations.size(),
      artboard,
      useFirstAsBaseline);
  return animationReset;
}

std::vector<std::unique_ptr<AnimationReset>> AnimationResetFactory::m_resources;

std::mutex AnimationResetFactory::m_mutex;

void AnimationResetFactory::release(std::unique_ptr<AnimationReset> value)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  value->clear();
  m_resources.push_back(std::move(value));
}
