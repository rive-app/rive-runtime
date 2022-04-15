#include "rive/animation/state_machine_event.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_event_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/event_input_change.hpp"
#include "rive/animation/state_machine.hpp"

using namespace rive;

StatusCode EventInputChange::import(ImportStack& importStack) {
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr) {
        return StatusCode::MissingObject;
    }

    auto stateMachineEventImporter =
        importStack.latest<StateMachineEventImporter>(StateMachineEventBase::typeKey);
    if (stateMachineEventImporter == nullptr) {
        return StatusCode::MissingObject;
    }

    if (!validateInputType(stateMachineImporter->stateMachine()->input((size_t)inputId()))) {
        return StatusCode::InvalidObject;
    }
    stateMachineEventImporter->addInputChange(this);
    return Super::import(importStack);
}
