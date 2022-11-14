#ifndef _RIVE_BLEND_ANIMATION_HPP_
#define _RIVE_BLEND_ANIMATION_HPP_
#include "rive/generated/animation/blend_animation_base.hpp"
namespace rive
{
class LinearAnimation;
class BlendAnimation : public BlendAnimationBase
{
private:
    static LinearAnimation m_EmptyAnimation;
    LinearAnimation* m_Animation = nullptr;

public:
    const LinearAnimation* animation() const
    {
        return m_Animation == nullptr ? &m_EmptyAnimation : m_Animation;
    }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif