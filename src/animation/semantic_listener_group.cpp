#include "rive/animation/semantic_listener_group.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/semantic/semantic_data.hpp"

using namespace rive;

SemanticListenerGroup::SemanticListenerGroup(
    SemanticData* semanticData,
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance) :
    m_semanticData(semanticData),
    m_listener(listener),
    m_stateMachineInstance(stateMachineInstance),
    m_isTapListener(listener != nullptr &&
                    listener->hasListener(ListenerType::semanticTap)),
    m_isIncreaseListener(listener != nullptr &&
                         listener->hasListener(ListenerType::semanticIncrease)),
    m_isDecreaseListener(listener != nullptr &&
                         listener->hasListener(ListenerType::semanticDecrease))
{
    if (m_semanticData != nullptr)
    {
        m_semanticData->addSemanticListener(this);
    }
}

SemanticListenerGroup::~SemanticListenerGroup()
{
    if (m_semanticData != nullptr)
    {
        m_semanticData->removeSemanticListener(this);
    }
}

void SemanticListenerGroup::onSemanticTap()
{
    if (m_isTapListener)
    {
        m_stateMachineInstance->queueSemanticEvent(this,
                                                   SemanticActionType::tap);
    }
}

void SemanticListenerGroup::onSemanticIncrease()
{
    if (m_isIncreaseListener)
    {
        m_stateMachineInstance->queueSemanticEvent(
            this,
            SemanticActionType::increase);
    }
}

void SemanticListenerGroup::onSemanticDecrease()
{
    if (m_isDecreaseListener)
    {
        m_stateMachineInstance->queueSemanticEvent(
            this,
            SemanticActionType::decrease);
    }
}
