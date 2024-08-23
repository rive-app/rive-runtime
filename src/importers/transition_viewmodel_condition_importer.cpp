#include "rive/importers/transition_viewmodel_condition_importer.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/data_bind/data_bind.hpp"

using namespace rive;

TransitionViewModelConditionImporter::TransitionViewModelConditionImporter(
    TransitionViewModelCondition* transitionViewModelCondition) :
    m_TransitionViewModelCondition(transitionViewModelCondition)
{}

void TransitionViewModelConditionImporter::setComparator(TransitionComparator* comparator)
{
    m_TransitionViewModelCondition->comparator(comparator);
}