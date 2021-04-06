#include "animation/state_transition.hpp"
#include "importers/import_stack.hpp"
#include "importers/layer_state_importer.hpp"
#include "animation/layer_state.hpp"

using namespace rive;

StatusCode StateTransition::onAddedDirty(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode StateTransition::onAddedClean(CoreContext* context)
{
	return StatusCode::Ok;
}

StatusCode StateTransition::import(ImportStack& importStack)
{
	auto stateImporter =
	    importStack.latest<LayerStateImporter>(LayerState::typeKey);
	if (stateImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	stateImporter->addTransition(this);
	return Super::import(importStack);
}