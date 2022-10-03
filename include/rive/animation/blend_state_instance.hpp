#ifndef _RIVE_BLEND_STATE_INSTANCE_HPP_
#define _RIVE_BLEND_STATE_INSTANCE_HPP_

#include <string>
#include <vector>
#include "rive/animation/state_instance.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/linear_animation_instance.hpp"

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
    }

    bool keepGoing() const override { return m_KeepGoing; }

    void advance(float seconds, Span<SMIInput*>) override
    {
        m_KeepGoing = false;
        for (auto& animation : m_AnimationInstances)
        {
            if (animation.m_AnimationInstance.advance(seconds))
            {
                m_KeepGoing = true;
            }
        }
    }

    void apply(float mix) override
    {
        for (auto& animation : m_AnimationInstances)
        {
            float m = mix * animation.m_Mix;
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