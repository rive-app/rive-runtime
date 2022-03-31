#include "rive/importers/state_machine_event_importer.hpp"
#include "rive/animation/state_machine_event.hpp"

using namespace rive;

StateMachineEventImporter::StateMachineEventImporter(StateMachineEvent* event) :
    m_StateMachineEvent(event) {}

void StateMachineEventImporter::addInputChange(EventInputChange* change) {
    m_StateMachineEvent->addInputChange(change);
}

StatusCode StateMachineEventImporter::resolve() { return StatusCode::Ok; }