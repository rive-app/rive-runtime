#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/animation/state_machine_listener.hpp"

using namespace rive;

StateMachineListenerImporter::StateMachineListenerImporter(StateMachineListener* listener) :
    m_StateMachineListener(listener) {}

void StateMachineListenerImporter::addInputChange(ListenerInputChange* change) {
    m_StateMachineListener->addInputChange(change);
}

StatusCode StateMachineListenerImporter::resolve() { return StatusCode::Ok; }