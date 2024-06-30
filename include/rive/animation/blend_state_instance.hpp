#ifndef _RIVE_BLEND_STATE_INSTANCE_HPP_
#define _RIVE_BLEND_STATE_INSTANCE_HPP_

#include <string>
#include <vector>
#include "rive/animation/state_instance.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/layer_state_flags.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/animation_reset.hpp"
#include "rive/animation/animation_reset_factory.hpp"

namespace rive
{
class AnimationState;

template <class K, class T> class BlendStateInstance;
template <class T> class BlendStateAnimationInstance
{
    template <class A, class B> friend class BlendStateInstance;

private:
    const T* m_BlendAnimation;
    LinearAnimationInstance m_AnimationInstance;
    float m_Mix = 0.0f;

public:
    const T* blendAnimation() const { return m_BlendAnimation; }
    const LinearAnimationInstance* animationInstance() const { return &m_AnimationInstance; }

    BlendStateAnimationInstance(const T* blendAnimation, ArtboardInstance* instance) :
        m_BlendAnimation(blendAnimation), m_AnimationInstance(blendAnimation->animation(), instance)
    {}

    void mix(float value) { m_Mix = value; }
};

template <class K, class T> class BlendStateInstance : public StateInstance
{
protected:
    std::vector<BlendStateAnimationInstance<T>> m_AnimationInstances;
    bool m_KeepGoing = true;

public:
    BlendStateInstance(const K* blendState, ArtboardInstance* instance) : StateInstance(blendState)
    {
        m_AnimationInstances.reserve(blendState->animations().size());

        for (auto blendAnimation : blendState->animations())
        {
            m_AnimationInstances.emplace_back(
                BlendStateAnimationInstance<T>(static_cast<T*>(blendAnimation), instance));
        }
        if ((static_cast<LayerStateFlags>(blendState->flags()) & LayerStateFlags::Reset) ==
            LayerStateFlags::Reset)
        {
            auto animations = std::vector<const LinearAnimation*>();
            for (auto blendAnimation : blendState->animations())
            {
                animations.push_back(blendAnimation->animation());
            }
        }
    }

    bool keepGoing() const override { return m_KeepGoing; }

    void advance(float seconds, StateMachineInstance* stateMachineInstance) override
    {
        // NOTE: we are intentionally ignoring the animationInstances' keepGoing
        // return value.
        // Blend states need to keep blending forever, as even if the animation
        // does not change the mix values may
        for (auto& animation : m_AnimationInstances)
        {
            if (animation.m_AnimationInstance.keepGoing())
            {
                // Should animations with m_Mix == 0.0 advance? They will
                // trigger events and the event properties (if any) will not be
                // updated by animationInstance.apply
                animation.m_AnimationInstance.advance(seconds, stateMachineInstance);
            }
        }
    }

    void apply(ArtboardInstance* instance, float mix) override
    {
        for (auto& animation : m_AnimationInstances)
        {
            float m = mix * animation.m_Mix;
            if (m == 0.0f)
            {
                continue;
            }
            animation.m_AnimationInstance.apply(m);
        }
    }

    // Find the animationInstance that corresponds to the blendAnimation.
    const LinearAnimationInstance* animationInstance(const BlendAnimation* blendAnimation) const
    {
        for (auto& animation : m_AnimationInstances)
        {
            if (animation.m_BlendAnimation == blendAnimation)
            {
                return animation.animationInstance();
            }
        }
        return nullptr;
    }
};
} // namespace rive
#endif