#include "rive/animation/state_machine_listener.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/listener_input_change.hpp"
#include "rive/animation/listener_types/listener_input_type.hpp"
#include <array>

using namespace rive;

// The listener types that hit-test a pointer against a target. A listener with
// any of these needs its target present (and, for a LayoutComponent, sortable)
// in the state machine's hit lookup.
static constexpr std::array<ListenerType, 9> kPointerHitListenerTypes = {
    ListenerType::enter,
    ListenerType::exit,
    ListenerType::down,
    ListenerType::up,
    ListenerType::move,
    ListenerType::click,
    ListenerType::dragStart,
    ListenerType::dragEnd,
    ListenerType::drag,
};

StateMachineListener::StateMachineListener() {}
StateMachineListener::~StateMachineListener() {}

bool StateMachineListener::hasListener(ListenerType listenerType) const
{
    for (auto& listenerInputType : m_listenerInputTypes)
    {
        if (listenerInputType->listenerTypeValue() == (int)listenerType)
        {
            return true;
        }
    }
    return false;
}

bool StateMachineListener::hasListeners(
    Span<const ListenerType> listenerTypes) const
{
    for (auto listenerType : listenerTypes)
    {
        if (hasListener(listenerType))
        {
            return true;
        }
    }
    return false;
}

void StateMachineListener::addAction(std::unique_ptr<ListenerAction> action)
{
    m_actions.push_back(std::move(action));
}

void StateMachineListener::addListenerInputType(
    std::unique_ptr<ListenerInputType> listenerInputType)
{
    m_listenerInputTypes.push_back(std::move(listenerInputType));
}

StatusCode StateMachineListener::import(ImportStack& importStack)
{
    auto stateMachineImporter =
        importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // Handing off ownership of this!
    stateMachineImporter->addListener(
        std::unique_ptr<StateMachineListener>(this));
    return Super::import(importStack);
}

bool StateMachineListener::hasPointerListeners() const
{
    return hasListeners(kPointerHitListenerTypes);
}

StatusCode StateMachineListener::onAddedClean(CoreContext* context)
{
    // A pointer listener that targets a LayoutComponent registers the layout's
    // proxy in the hit lookup (StateMachineInstance). For that proxy to sort
    // correctly against overlapping drawables it must be in the draw order, so
    // stamp the target here — before the artboard's one-time proxy injection.
    // This hook only runs on the source artboard; LayoutComponent::clone()
    // carries the flag to instances.
    if (hasPointerListeners())
    {
        auto* target = context->resolve(targetId());
        if (target != nullptr && target->is<LayoutComponent>())
        {
            target->as<LayoutComponent>()->markListenerTarget();
        }
    }
    return Super::onAddedClean(context);
}

const ListenerAction* StateMachineListener::action(size_t index) const
{
    if (index < m_actions.size())
    {
        return m_actions[index].get();
    }
    return nullptr;
}

const ListenerInputType* StateMachineListener::listenerInputType(
    size_t index) const
{
    if (index < m_listenerInputTypes.size())
    {
        return m_listenerInputTypes[index].get();
    }
    return nullptr;
}

void StateMachineListener::performChanges(
    StateMachineInstance* stateMachineInstance,
    const ListenerInvocation& invocation) const
{
    for (auto& action : m_actions)
    {
        action->perform(stateMachineInstance, invocation);
    }
}