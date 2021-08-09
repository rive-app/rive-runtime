#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/blend_state_1d_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

StateInstance* BlendState1D::makeInstance() const
{
	return new BlendState1DInstance(this);
}

StatusCode BlendState1D::import(ImportStack& importStack)
{
	auto stateMachineImporter =
	    importStack.latest<StateMachineImporter>(StateMachine::typeKey);
	if (stateMachineImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}

	// Make sure the inputId doesn't overflow the input buffer.
	if (inputId() < 0 ||
	    inputId() >= stateMachineImporter->stateMachine()->inputCount())
	{
		return StatusCode::InvalidObject;
	}
	auto input = stateMachineImporter->stateMachine()->input((size_t)inputId());
	if (input == nullptr || !input->is<StateMachineNumber>())
	{
		return StatusCode::InvalidObject;
	}
	return Super::import(importStack);
}