#include "animation/layer_state.hpp"
#include "animation/transition_bool_condition.hpp"
#include "importers/import_stack.hpp"
#include "importers/state_machine_layer_importer.hpp"
#include "generated/animation/state_machine_layer_base.hpp"

using namespace rive;

using namespace rive;

StatusCode LayerState::onAddedDirty(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode LayerState::onAddedClean(CoreContext* context)
{
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