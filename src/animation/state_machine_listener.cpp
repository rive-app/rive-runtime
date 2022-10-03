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
    m_Actions.push_back(std::move(action));
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
    if (index < m_Actions.size())
    {
        return m_Actions[index].get();
    }
    return nullptr;
}

StatusCode StateMachineListener::onAddedClean(CoreContext* context)
{
    auto artboard = static_cast<Artboard*>(context);
    auto target = artboard->resolve(targetId());

    for (auto core : artboard->objects())
    {
        if (core == nullptr)
        {
            continue;
        }

        // Iterate artboard to find Shapes that are parented to the target
        if (core->is<Shape>())
        {
            auto shape = core->as<Shape>();

            for (ContainerComponent* component = shape; component != nullptr;
                 component = component->parent())
            {
                if (component == target)
                {
                    auto index = artboard->idOf(shape);
                    if (index != 0)
                    {
                        m_HitShapesIds.push_back(index);
                    }
                    break;
                }
            }
        }
    }

    return Super::onAddedClean(context);
}

void StateMachineListener::performChanges(StateMachineInstance* stateMachineInstance,
                                          Vec2D position) const
{
    for (auto& action : m_Actions)
    {
        action->perform(stateMachineInstance, position);
    }
}