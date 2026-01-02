#include "rive/animation/state_machine_fire_trigger.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

void StateMachineFireTrigger::perform(
    StateMachineInstance* stateMachineInstance) const
{
    auto dataContext = stateMachineInstance->dataContext();
    if (dataContext != nullptr && m_dataBindPath)
    {
        auto vmProp = dataContext->getViewModelProperty(m_dataBindPath);
        if (vmProp && vmProp->is<ViewModelInstanceTrigger>())
        {
            vmProp->as<ViewModelInstanceTrigger>()->trigger();
        }
    }
}

StatusCode StateMachineFireTrigger::import(ImportStack& importStack)
{
    importDataBindPath(importStack);
    return Super::import(importStack);
}

void StateMachineFireTrigger::decodeViewModelPathIds(Span<const uint8_t> value)
{
    decodeDataBindPath(value);
}

void StateMachineFireTrigger::copyViewModelPathIds(
    const StateMachineFireTriggerBase& object)
{
    copyDataBindPath(object.as<StateMachineFireTrigger>()->dataBindPath());
}