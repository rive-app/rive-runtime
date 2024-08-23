#include "rive/animation/state_machine.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_listener.hpp"

using namespace rive;

StateMachine::StateMachine() {}

StateMachine::~StateMachine() {}

StatusCode StateMachine::onAddedDirty(CoreContext* context)
{
    StatusCode code;
    for (auto& object : m_Inputs)
    {
        if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    for (auto& object : m_Layers)
    {
        if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    for (auto& object : m_Listeners)
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
    for (auto& object : m_Inputs)
    {
        if ((code = object->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    for (auto& object : m_Layers)
    {
        if ((code = object->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    for (auto& object : m_Listeners)
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
    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addStateMachine(this);
    return Super::import(importStack);
}

void StateMachine::addLayer(std::unique_ptr<StateMachineLayer> layer)
{
    m_Layers.push_back(std::move(layer));
}

void StateMachine::addInput(std::unique_ptr<StateMachineInput> input)
{
    m_Inputs.push_back(std::move(input));
}

void StateMachine::addListener(std::unique_ptr<StateMachineListener> listener)
{
    m_Listeners.push_back(std::move(listener));
}

void StateMachine::addDataBind(std::unique_ptr<DataBind> dataBind)
{
    m_dataBinds.push_back(std::move(dataBind));
}

const StateMachineInput* StateMachine::input(std::string name) const
{
    for (auto& input : m_Inputs)
    {
        if (input->name() == name)
        {
            return input.get();
        }
    }
    return nullptr;
}

const StateMachineInput* StateMachine::input(size_t index) const
{
    if (index < m_Inputs.size())
    {
        return m_Inputs[index].get();
    }
    return nullptr;
}

const StateMachineLayer* StateMachine::layer(std::string name) const
{
    for (auto& layer : m_Layers)
    {
        if (layer->name() == name)
        {
            return layer.get();
        }
    }
    return nullptr;
}

const StateMachineLayer* StateMachine::layer(size_t index) const
{
    if (index < m_Layers.size())
    {
        return m_Layers[index].get();
    }
    return nullptr;
}

const StateMachineListener* StateMachine::listener(size_t index) const
{
    if (index < m_Listeners.size())
    {
        return m_Listeners[index].get();
    }
    return nullptr;
}

const DataBind* StateMachine::dataBind(size_t index) const
{
    if (index < m_dataBinds.size())
    {
        return m_dataBinds[index].get();
    }
    return nullptr;
}