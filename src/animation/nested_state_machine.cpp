#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/hit_result.hpp"

using namespace rive;

NestedStateMachine::NestedStateMachine() {}
NestedStateMachine::~NestedStateMachine() {}

void NestedStateMachine::advance(float elapsedSeconds)
{
    if (m_StateMachineInstance != nullptr)
    {
        m_StateMachineInstance->advance(elapsedSeconds);
    }
}

void NestedStateMachine::initializeAnimation(ArtboardInstance* artboard)
{
    m_StateMachineInstance = artboard->stateMachineAt(animationId());
    auto count = m_nestedInputs.size();
    for (size_t i = 0; i < count; i++)
    {
        auto nestedInput = m_nestedInputs[i];
        if (nestedInput->is<NestedBool>() || nestedInput->is<NestedNumber>())
        {
            nestedInput->applyValue();
        }
    }
    m_nestedInputs.clear();
}

StateMachineInstance* NestedStateMachine::stateMachineInstance()
{
    return m_StateMachineInstance.get();
}

HitResult NestedStateMachine::pointerMove(Vec2D position)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerMove(position);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerDown(Vec2D position)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerDown(position);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerUp(Vec2D position)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerUp(position);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerExit(Vec2D position)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerExit(position);
    }
    return HitResult::none;
}

void NestedStateMachine::addNestedInput(NestedInput* input) { m_nestedInputs.push_back(input); }