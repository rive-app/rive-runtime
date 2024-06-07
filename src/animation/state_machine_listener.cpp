#include "rive/animation/state_machine_listener.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/listener_input_change.hpp"

using namespace rive;

StateMachineListener::StateMachineListener() {}
StateMachineListener::~StateMachineListener() {}

void StateMachineListener::addAction(std::unique_ptr<ListenerAction> action)
{
    m_actions.push_back(std::move(action));
}

StatusCode StateMachineListener::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // Handing off ownership of this!
    stateMachineImporter->addListener(std::unique_ptr<StateMachineListener>(this));
    return Super::import(importStack);
}

const ListenerAction* StateMachineListener::action(size_t index) const
{
    if (index < m_actions.size())
    {
        return m_actions[index].get();
    }
    return nullptr;
}

void StateMachineListener::performChanges(StateMachineInstance* stateMachineInstance,
                                          Vec2D position,
                                          Vec2D previousPosition) const
{
    for (auto& action : m_actions)
    {
        action->perform(stateMachineInstance, position, previousPosition);
    }
}