#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

NestedStateMachine::NestedStateMachine() {}
NestedStateMachine::~NestedStateMachine() {}

void NestedStateMachine::advance(float elapsedSeconds) {
    if (m_StateMachineInstance != nullptr) {
        m_StateMachineInstance->advance(elapsedSeconds);
    }
}

void NestedStateMachine::initializeAnimation(ArtboardInstance* artboard) {
    m_StateMachineInstance = artboard->stateMachineAt(animationId());
}

StateMachineInstance* NestedStateMachine::stateMachineInstance() {
    return m_StateMachineInstance.get();
}

void NestedStateMachine::pointerMove(Vec2D position) {
    if (m_StateMachineInstance != nullptr) {
        m_StateMachineInstance->pointerMove(position);
    }
}

void NestedStateMachine::pointerDown(Vec2D position) {
    if (m_StateMachineInstance != nullptr) {
        m_StateMachineInstance->pointerDown(position);
    }
}

void NestedStateMachine::pointerUp(Vec2D position) {
    if (m_StateMachineInstance != nullptr) {
        m_StateMachineInstance->pointerUp(position);
    }
}