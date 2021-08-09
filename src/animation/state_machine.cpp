#include "rive/animation/state_machine.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_input.hpp"

using namespace rive;

StateMachine::~StateMachine()
{
	for (auto object : m_Inputs)
	{
		delete object;
	}
	for (auto object : m_Layers)
	{
		delete object;
	}
}

StatusCode StateMachine::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_Inputs)
	{
		if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	for (auto object : m_Layers)
	{
		if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode StateMachine::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_Inputs)
	{
		if ((code = object->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	for (auto object : m_Layers)
	{
		if ((code = object->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

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

void StateMachine::addLayer(StateMachineLayer* layer)
{
	m_Layers.push_back(layer);
}

void StateMachine::addInput(StateMachineInput* input)
{
	m_Inputs.push_back(input);
}

const StateMachineInput* StateMachine::input(std::string name) const
{
	for (auto input : m_Inputs)
	{
		if (input->name() == name)
		{
			return input;
		}
	}
	return nullptr;
}

const StateMachineInput* StateMachine::input(size_t index) const
{
	if (index >= 0 && index < m_Inputs.size())
	{
		return m_Inputs[index];
	}
	return nullptr;
}

const StateMachineLayer* StateMachine::layer(std::string name) const
{
	for (auto layer : m_Layers)
	{
		if (layer->name() == name)
		{
			return layer;
		}
	}
	return nullptr;
}

const StateMachineLayer* StateMachine::layer(size_t index) const
{
	if (index >= 0 && index < m_Layers.size())
	{
		return m_Layers[index];
	}
	return nullptr;
}