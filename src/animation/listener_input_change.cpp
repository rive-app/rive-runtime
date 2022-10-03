#include "rive/animation/state_machine_listener.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/listener_input_change.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

StatusCode ListenerInputChange::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    if (!validateInputType(stateMachineImporter->stateMachine()->input((size_t)inputId())))
    {
        return StatusCode::InvalidObject;
    }
    return Super::import(importStack);
}
