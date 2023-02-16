#ifndef _RIVE_ANIMATION_STATE_HPP_
#define _RIVE_ANIMATION_STATE_HPP_
#include "rive/generated/animation/animation_state_base.hpp"
#include <stdio.h>
namespace rive
{
class LinearAnimation;
class ArtboardInstance;
class StateMachineLayerImporter;

class AnimationState : public AnimationStateBase
{
    friend class StateMachineLayerImporter;

private:
    LinearAnimation* m_Animation = nullptr;

public:
    const LinearAnimation* animation() const { return m_Animation; }

#ifdef TESTING
    void animation(LinearAnimation* animation);
#endif

    std::unique_ptr<StateInstance> makeInstance(ArtboardInstance*) const override;
};
} // namespace rive

#endif