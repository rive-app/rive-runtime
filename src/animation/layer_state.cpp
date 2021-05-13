#include "animation/layer_state.hpp"
#include "animation/transition_bool_condition.hpp"
#include "importers/import_stack.hpp"
#include "importers/state_machine_layer_importer.hpp"
#include "generated/animation/state_machine_layer_base.hpp"
#include "animation/state_transition.hpp"

using namespace rive;

LayerState::~LayerState()
{
	for (auto transition : m_Transitions)
	{
		delete transition;
	}
}

StatusCode LayerState::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto transition : m_Transitions)
	{
		if ((code = transition->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode LayerState::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto transition : m_Transitions)
	{
		if ((code = transition->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode LayerState::import(ImportStack& importStack)
{
	auto layerImporter = importStack.latest<StateMachineLayerImporter>(
	    StateMachineLayerBase::typeKey);
	if (layerImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	layerImporter->addState(this);
	return Super::import(importStack);
}

void LayerState::addTransition(StateTransition* transition)
{
	m_Transitions.push_back(transition);
}