#include "rive/animation/state_machine_listener.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/listener_input_change.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

StatusCode ListenerAction::import(ImportStack& importStack)
{
    auto stateMachineListenerImporter =
        importStack.latest<StateMachineListenerImporter>(StateMachineListenerBase::typeKey);
    if (stateMachineListenerImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    stateMachineListenerImporter->addAction(std::unique_ptr<ListenerAction>(this));
    return Super::import(importStack);
}
