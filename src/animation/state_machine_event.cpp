#include "rive/animation/state_machine_event.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"

using namespace rive;

void StateMachineEvent::addInputChange(EventInputChange* inputChange) {
    m_InputChanges.push_back(inputChange);
}

StatusCode StateMachineEvent::import(ImportStack& importStack) {
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr) {
        return StatusCode::MissingObject;
    }
    stateMachineImporter->addEvent(this);
    return Super::import(importStack);
}

const EventInputChange* StateMachineEvent::inputChange(size_t index) const {
    if (index < m_InputChanges.size()) {
        return m_InputChanges[index];
    }
    return nullptr;
}