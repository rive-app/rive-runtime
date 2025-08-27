#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/importers/state_transition_importer.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

TransitionViewModelCondition::~TransitionViewModelCondition()
{
    if (m_leftComparator != nullptr)
    {
        delete m_leftComparator;
        m_leftComparator = nullptr;
    }
    if (m_rightComparator != nullptr)
    {
        delete m_rightComparator;
        m_rightComparator = nullptr;
    }
}

bool TransitionViewModelCondition::evaluate(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    if (leftComparator() != nullptr && rightComparator() != nullptr)
    {
        return leftComparator()->compare(rightComparator(),
                                         op(),
                                         stateMachineInstance,
                                         layerInstance);
    }
    return false;
}

void TransitionViewModelCondition::useInLayer(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    if (leftComparator() != nullptr)
    {
        return leftComparator()->useInLayer(stateMachineInstance,
                                            layerInstance);
    }
}