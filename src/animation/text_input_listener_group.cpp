#include "rive/animation/text_input_listener_group.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

TextInputListenerGroup::TextInputListenerGroup(
    TextInput* textInput,
    StateMachineInstance* stateMachineInstance) :
    // Pass nullptr for the listener - we won't use the base class's
    // listener-based logic, just the click phase tracking
    ListenerGroup(nullptr), m_textInput(textInput)
{}

ProcessEventResult TextInputListenerGroup::processEvent(
    Component* component,
    Vec2D position,
    int pointerId,
    ListenerType hitEvent,
    bool canHit,
    float timeStamp,
    StateMachineInstance* stateMachineInstance)
{
    // We implement our own click phase tracking instead of calling base class
    // because base class processEvent accesses m_listener which is nullptr
    auto* pData = pointerData(pointerId);
    GestureClickPhase prevPhase = pData->phase;

    // Update hover state based on canHit
    if (!canHit && pData->isHovered)
    {
        pData->isHovered = false;
    }

    bool isGroupHovered = canHit ? pData->isHovered : false;

    // Update click phase
    if (isGroupHovered)
    {
        if (hitEvent == ListenerType::down)
        {
            pData->phase = GestureClickPhase::down;
        }
        else if (hitEvent == ListenerType::up &&
                 pData->phase == GestureClickPhase::down)
        {
            pData->phase = GestureClickPhase::clicked;
        }
    }
    else
    {
        if (hitEvent == ListenerType::down || hitEvent == ListenerType::up)
        {
            pData->phase = GestureClickPhase::out;
        }
    }

    GestureClickPhase newPhase = pData->phase;

    // Handle drag start: pointer went down while hovering this text input
    if (prevPhase != GestureClickPhase::down &&
        newPhase == GestureClickPhase::down)
    {
        m_textInput->startDrag(position);
        m_isDragging = true;

        auto* manager = stateMachineInstance->focusManager();
        if (manager != nullptr)
        {
            for (auto* child : m_textInput->children())
            {
                if (child->is<FocusData>())
                {
                    auto focusNode = child->as<FocusData>()->focusNode();
                    if (focusNode != nullptr)
                    {
                        manager->setFocus(focusNode);
                    }
                    break;
                }
            }
        }
        return ProcessEventResult::scroll;
    }
    // Handle drag continue: pointer is moving while down
    else if (hitEvent == ListenerType::move &&
             newPhase == GestureClickPhase::down && m_isDragging)
    {
        m_textInput->drag(position);
        return ProcessEventResult::scroll;
    }
    // Handle drag end: pointer released or moved out while dragging
    else if (prevPhase == GestureClickPhase::down &&
             (newPhase == GestureClickPhase::clicked ||
              newPhase == GestureClickPhase::out))
    {
        if (m_isDragging)
        {
            m_textInput->endDrag(position);
            m_isDragging = false;
        }
    }

    return ProcessEventResult::none;
}
