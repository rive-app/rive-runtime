#include "rive/importers/state_machine_layer_importer.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/artboard.hpp"

using namespace rive;
StateMachineLayerImporter::StateMachineLayerImporter(StateMachineLayer* layer,
                                                     const Artboard* artboard) :
    m_Layer(layer), m_Artboard(artboard)
{}
void StateMachineLayerImporter::addState(LayerState* state) { m_Layer->addState(state); }

StatusCode StateMachineLayerImporter::resolve()
{

    for (auto state : m_Layer->m_States)
    {
        if (state->is<AnimationState>())
        {
            auto animationState = state->as<AnimationState>();

            if (animationState->animationId() < m_Artboard->animationCount())
            {
                animationState->m_Animation = m_Artboard->animation(animationState->animationId());
                if (animationState->m_Animation == nullptr)
                {
                    return StatusCode::MissingObject;
                }
            }
        }
        for (auto transition : state->m_Transitions)
        {
            if ((size_t)transition->stateToId() < m_Layer->m_States.size())
            {
                transition->m_StateTo = m_Layer->m_States[transition->stateToId()];
            }
            else
            {
                return StatusCode::InvalidObject;
            }
        }
    }
    return StatusCode::Ok;
}

bool StateMachineLayerImporter::readNullObject()
{
    // Add an 'empty' generic state that can be transitioned to/from but doesn't
    // effectively do anything. This allows us to deal with unexpected new state
    // types the runtime won't be able to understand. It'll still be able to
    // make use of the state but it won't do anything visually.
    addState(new LayerState());
    return true;
}
