#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/animation/state_machine_listener.hpp"

using namespace rive;

StateMachineListenerImporter::StateMachineListenerImporter(StateMachineListener* listener) :
    m_StateMachineListener(listener) {}

void StateMachineListenerImporter::addAction(ListenerAction* action) {
    m_StateMachineListener->addAction(action);
}

StatusCode StateMachineListenerImporter::resolve() { return StatusCode::Ok; }