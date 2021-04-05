#include "animation/state_machine.hpp"
#include "artboard.hpp"
#include "importers/artboard_importer.hpp"

using namespace rive;

StatusCode StateMachine::import(ImportStack& importStack)
{
	auto artboardImporter =
	    importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
	if (artboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	artboardImporter->addStateMachine(this);
	return Super::import(importStack);
}