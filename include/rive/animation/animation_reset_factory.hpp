#ifndef _RIVE_ANIMATION_RESET_FACTORY_HPP_
#define _RIVE_ANIMATION_RESET_FACTORY_HPP_

#include <string>
#include <mutex>
#include "rive/animation/animation_reset.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"

namespace rive
{

class AnimationResetFactory
{
    static std::vector<std::unique_ptr<AnimationReset>> m_resources;
    static std::mutex m_mutex;

private:
    static void fromState(StateInstance* stateInstance,
                          std::vector<const LinearAnimation*>& animations);

public:
    static std::unique_ptr<AnimationReset> getInstance();
    static std::unique_ptr<AnimationReset> fromStates(StateInstance* stateFrom,
                                                      StateInstance* currentState,
                                                      ArtboardInstance* artboard);
    static std::unique_ptr<AnimationReset> fromAnimations(
        std::vector<const LinearAnimation*>& animations,
        ArtboardInstance* artboard,
        bool useFirstAsBaseline);
    static void release(std::unique_ptr<AnimationReset> value);
#ifdef TESTING
    // Used in testing to check pooled resources;
    static int resourcesCount() { return m_resources.size(); };
    static void releaseResources() { m_resources.clear(); };
#endif
};
} // namespace rive
#endif