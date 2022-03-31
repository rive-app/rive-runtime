#include "rive/animation/state_machine_event.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_event_importer.hpp"
#include "rive/animation/event_input_change.hpp"
#include "rive/generated/animation/state_machine_base.hpp"

using namespace rive;

StatusCode EventInputChange::import(ImportStack& importStack) {
    auto stateMachineEventImporter =
        importStack.latest<StateMachineEventImporter>(StateMachineEventBase::typeKey);
    if (stateMachineEventImporter == nullptr) {
        return StatusCode::MissingObject;
    }
    stateMachineEventImporter->addInputChange(this);
    return Super::import(importStack);
}
