#include "rive/animation/listener_action.hpp"
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/importers/state_machine_listener_importer.hpp"
#include "rive/animation/state_machine_listener.hpp"

using namespace rive;

StateMachineListenerImporter::StateMachineListenerImporter(
    StateMachineListener* listener) :
    m_StateMachineListener(listener)
{}

void StateMachineListenerImporter::addAction(
    std::unique_ptr<ListenerAction> action)
{
    m_StateMachineListener->addAction(std::move(action));
}

void StateMachineListenerImporter::addListenerInputType(
    std::unique_ptr<ListenerInputType> listenerInputType)
{
    m_StateMachineListener->addListenerInputType(std::move(listenerInputType));
}

StatusCode StateMachineListenerImporter::resolve() { return StatusCode::Ok; }