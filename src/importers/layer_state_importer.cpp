#include "rive/importers/layer_state_importer.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/blend_state_transition.hpp"

using namespace rive;

LayerStateImporter::LayerStateImporter(LayerState* state) : m_State(state) {}
void LayerStateImporter::addTransition(StateTransition* transition)
{
    m_State->addTransition(transition);
}

bool LayerStateImporter::addBlendAnimation(BlendAnimation* animation)
{
    if (!m_State->is<BlendState>())
    {
        return false;
    }
    auto blendState = m_State->as<BlendState>();

    blendState->addAnimation(animation);
    return true;
}

StatusCode LayerStateImporter::resolve()
{
    if (m_State->is<BlendState>())
    {
        auto blendState = m_State->as<BlendState>();
        for (auto transition : blendState->m_Transitions)
        {
            if (!transition->is<BlendStateTransition>())
            {
                continue;
            }

            auto blendStateTransition = transition->as<BlendStateTransition>();
            size_t exitId = blendStateTransition->exitBlendAnimationId();
            if (exitId < blendState->m_Animations.size())
            {
                blendStateTransition->m_ExitBlendAnimation = blendState->m_Animations[exitId];
            }
        }
    }
    return StatusCode::Ok;
}
