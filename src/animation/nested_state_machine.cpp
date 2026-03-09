#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/focus_data.hpp"
#include "rive/hit_result.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

NestedStateMachine::NestedStateMachine() {}
NestedStateMachine::~NestedStateMachine() {}

bool NestedStateMachine::advance(float elapsedSeconds, bool newFrame)
{
    bool keepGoing = false;
    if (m_StateMachineInstance != nullptr)
    {
        keepGoing = m_StateMachineInstance->advance(elapsedSeconds, newFrame);
    }
    return keepGoing;
}

void NestedStateMachine::initializeAnimation(ArtboardInstance* artboard)
{
    m_StateMachineInstance = artboard->stateMachineAt(animationId());
    if (m_StateMachineInstance != nullptr)
    {
        // Share parent's focus manager and build nested focus tree.
        // The parent of NestedStateMachine is the NestedArtboard.
        if (parent() != nullptr && parent()->is<NestedArtboard>())
        {
            auto* nestedArtboard = parent()->as<NestedArtboard>();
            auto* parentArtboard = nestedArtboard->artboard();
            if (parentArtboard != nullptr &&
                parentArtboard->focusManager() != nullptr)
            {
                auto* parentFM = parentArtboard->focusManager();
                m_StateMachineInstance->setExternalFocusManager(parentFM);

                // Find closest focus node (handles artboard boundaries)
                auto parentNode =
                    FocusData::findClosestFocusNode(nestedArtboard);

                // Build nested artboard's focus tree under parent
                artboard->buildFocusTree(parentFM, parentNode);
            }
        }
    }

    auto count = m_nestedInputs.size();
    for (size_t i = 0; i < count; i++)
    {
        auto nestedInput = m_nestedInputs[i];
        if (nestedInput->is<NestedBool>() || nestedInput->is<NestedNumber>())
        {
            nestedInput->applyValue();
        }
    }
}

StateMachineInstance* NestedStateMachine::stateMachineInstance()
{
    return m_StateMachineInstance.get();
}

bool NestedStateMachine::hitTest(Vec2D position) const
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->hitTest(position);
    }
    return false;
}

HitResult NestedStateMachine::pointerMove(Vec2D position,
                                          float timeStamp,
                                          int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerMove(position,
                                                   timeStamp,
                                                   pointerId);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerDown(Vec2D position, int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerDown(position, pointerId);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerUp(Vec2D position, int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerUp(position, pointerId);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::pointerExit(Vec2D position, int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->pointerExit(position, pointerId);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::dragStart(Vec2D position,
                                        float timeStamp,
                                        int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->dragStart(position,
                                                 timeStamp,
                                                 true,
                                                 pointerId);
    }
    return HitResult::none;
}

HitResult NestedStateMachine::dragEnd(Vec2D position,
                                      float timeStamp,
                                      int pointerId)
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->dragEnd(position, timeStamp, pointerId);
    }
    return HitResult::none;
}

NestedInput* NestedStateMachine::input(size_t index)
{
    if (index < m_nestedInputs.size())
    {
        return m_nestedInputs[index];
    }
    return nullptr;
}

NestedInput* NestedStateMachine::input(std::string name)
{
    for (auto input : m_nestedInputs)
    {
        if (input->name() == name)
        {
            return input;
        }
    }
    return nullptr;
}

void NestedStateMachine::addNestedInput(NestedInput* input)
{
    m_nestedInputs.push_back(input);
}

void NestedStateMachine::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance)
{
    if (m_StateMachineInstance != nullptr)
    {
        m_StateMachineInstance->bindViewModelInstance(viewModelInstance);
    }
}

void NestedStateMachine::dataContext(rcp<DataContext> dataContext)
{
    if (m_StateMachineInstance != nullptr)
    {
        m_StateMachineInstance->dataContext(dataContext);
    }
}

void NestedStateMachine::clearDataContext()
{
    if (m_StateMachineInstance != nullptr)
    {
        m_StateMachineInstance->clearDataContext();
    }
}

bool NestedStateMachine::tryChangeState()
{
    if (m_StateMachineInstance != nullptr)
    {
        return m_StateMachineInstance->tryChangeState();
    }
    return false;
}