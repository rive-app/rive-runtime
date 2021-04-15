#include "importers/state_machine_layer_importer.hpp"
#include "importers/artboard_importer.hpp"
#include "animation/state_machine_layer.hpp"
#include "animation/animation_state.hpp"
#include "animation/state_transition.hpp"
#include "artboard.hpp"

using namespace rive;
StateMachineLayerImporter::StateMachineLayerImporter(
    StateMachineLayer* layer, ArtboardImporter* artboardImporter) :
    m_Layer(layer), m_ArtboardImporter(artboardImporter)
{
}
void StateMachineLayerImporter::addState(LayerState* state)
{
	m_Layer->addState(state);
}

StatusCode StateMachineLayerImporter::resolve()
{
	auto artboard = m_ArtboardImporter->artboard();
	for (auto state : m_Layer->m_States)
	{
		if (state->is<AnimationState>())
		{
			auto animationState = state->as<AnimationState>();

			if (animationState->animationId() != -1)
			{
				animationState->m_Animation =
				    artboard->animation(animationState->animationId());
				if (animationState->m_Animation == nullptr)
				{
					return StatusCode::MissingObject;
				}
			}
		}
		for (auto transition : state->m_Transitions)
		{
			if (transition->stateToId() < 0 ||
			    transition->stateToId() > m_Layer->m_States.size())
			{
				return StatusCode::InvalidObject;
			}
			transition->m_StateTo = m_Layer->m_States[transition->stateToId()];
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