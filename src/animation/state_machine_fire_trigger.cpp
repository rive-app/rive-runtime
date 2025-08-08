#include "rive/animation/state_machine_fire_trigger.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

void StateMachineFireTrigger::perform(
    StateMachineInstance* stateMachineInstance) const
{
    auto dataContext = stateMachineInstance->dataContext();
    if (dataContext != nullptr)
    {
        auto vmProp =
            dataContext->getViewModelProperty(m_viewModelPathIdsBuffer);
        if (vmProp && vmProp->is<ViewModelInstanceTrigger>())
        {
            vmProp->as<ViewModelInstanceTrigger>()->trigger();
        }
    }
}

void StateMachineFireTrigger::decodeViewModelPathIds(Span<const uint8_t> value)
{
    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        auto val = reader.readVarUintAs<uint32_t>();
        m_viewModelPathIdsBuffer.push_back(val);
    }
}

void StateMachineFireTrigger::copyViewModelPathIds(
    const StateMachineFireTriggerBase& object)
{
    m_viewModelPathIdsBuffer =
        object.as<StateMachineFireTrigger>()->m_viewModelPathIdsBuffer;
}