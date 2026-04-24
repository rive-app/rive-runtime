#include "rive/animation/semantic_listener_group.hpp"
#include "rive/animation/listener_types/listener_input_type_semantic.hpp"
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
    m_stateMachineInstance(stateMachineInstance)
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

void SemanticListenerGroup::queueIfListening(SemanticActionType actionType)
{
    if (m_listener != nullptr &&
        ListenerInputTypeSemantic::semanticListenerConstraintsMet(m_listener,
                                                                  actionType))
    {
        m_stateMachineInstance->queueSemanticEvent(this, actionType);
    }
}

void SemanticListenerGroup::onSemanticTap()
{
    queueIfListening(SemanticActionType::tap);
}

void SemanticListenerGroup::onSemanticIncrease()
{
    queueIfListening(SemanticActionType::increase);
}

void SemanticListenerGroup::onSemanticDecrease()
{
    queueIfListening(SemanticActionType::decrease);
}
