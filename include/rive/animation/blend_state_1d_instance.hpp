#ifndef _RIVE_BLEND_STATE_1D_INSTANCE_HPP_
#define _RIVE_BLEND_STATE_1D_INSTANCE_HPP_

#include "rive/animation/blend_state_instance.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_animation_1d.hpp"
#include "rive/animation/animation_reset.hpp"
#include "rive/animation/animation_reset_factory.hpp"

namespace rive
{
class BlendState1DInstance : public BlendStateInstance<BlendState1D, BlendAnimation1D>
{
private:
    BlendStateAnimationInstance<BlendAnimation1D>* m_From = nullptr;
    BlendStateAnimationInstance<BlendAnimation1D>* m_To = nullptr;
    std::unique_ptr<AnimationReset> m_AnimationReset;
    int animationIndex(float value);

public:
    BlendState1DInstance(const BlendState1D* blendState, ArtboardInstance* instance);
    ~BlendState1DInstance();
    void advance(float seconds, StateMachineInstance* stateMachineInstance) override;
    void apply(ArtboardInstance* instance, float mix) override;
};
} // namespace rive
#endif