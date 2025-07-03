#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/transition_viewmodel_condition_importer.hpp"

using namespace rive;

StatusCode TransitionComparator::import(ImportStack& importStack)
{
    auto transitionViewModelConditionImporter =
        importStack.latest<TransitionViewModelConditionImporter>(
            TransitionViewModelCondition::typeKey);
    if (transitionViewModelConditionImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    transitionViewModelConditionImporter->setComparator(this);
    return Super::import(importStack);
}

bool TransitionComparator::compareNumbers(float left,
                                          float right,
                                          TransitionConditionOp op)
{
    return compareComparables<float>(left, right, op);
}

bool TransitionComparator::compareStrings(std::string left,
                                          std::string right,
                                          TransitionConditionOp op)
{
    return compareComparables<std::string>(left, right, op);
}

bool TransitionComparator::compareBooleans(bool left,
                                           bool right,
                                           TransitionConditionOp op)
{
    return compareComparables<bool>(left, right, op);
}

bool TransitionComparator::compareEnums(uint16_t left,
                                        uint16_t right,
                                        TransitionConditionOp op)
{
    return compareComparables<uint16_t>(left, right, op);
}

bool TransitionComparator::compareTriggers(uint32_t left,
                                           uint32_t right,
                                           TransitionConditionOp op)
{
    return compareComparables<uint32_t>(left, right, op);
}

bool TransitionComparator::compareIds(uint32_t left,
                                      uint32_t right,
                                      TransitionConditionOp op)
{
    return compareComparables<uint32_t>(left, right, op);
}

bool TransitionComparator::compareColors(int left,
                                         int right,
                                         TransitionConditionOp op)
{
    return compareComparables<int>(left, right, op);
}

bool TransitionComparator::compare(
    TransitionComparator* comparand,
    TransitionConditionOp operation,
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance)
{
    return false;
}