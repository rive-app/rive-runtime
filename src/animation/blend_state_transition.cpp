#include "rive/artboard.hpp"
#include "rive/animation/blend_state_transition.hpp"
#include "rive/animation/blend_state_instance.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_state_direct.hpp"
#include "rive/animation/blend_state_1d_instance.hpp"
#include "rive/animation/blend_state_direct_instance.hpp"

using namespace rive;

const LinearAnimationInstance* BlendStateTransition::exitTimeAnimationInstance(
    const StateInstance* from) const
{
    if (from != nullptr)
    {
        switch (from->state()->coreType())
        {
            case BlendState1D::typeKey:

                return static_cast<const BlendState1DInstance*>(from)->animationInstance(
                    m_ExitBlendAnimation);

            case BlendStateDirect::typeKey:

                return static_cast<const BlendStateDirectInstance*>(from)->animationInstance(
                    m_ExitBlendAnimation);
        }
    }

    return nullptr;
}

const LinearAnimation* BlendStateTransition::exitTimeAnimation(const LayerState* from) const
{
    if (m_ExitBlendAnimation != nullptr)
    {
        return m_ExitBlendAnimation->animation();
    }
    return nullptr;
}