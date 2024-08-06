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

bool TransitionComparator::compareNumbers(float left, float right, TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return left == right;
        case TransitionConditionOp::notEqual:
            return left != right;
        case TransitionConditionOp::lessThanOrEqual:
            return left <= right;
        case TransitionConditionOp::lessThan:
            return left < right;
        case TransitionConditionOp::greaterThanOrEqual:
            return left >= right;
        case TransitionConditionOp::greaterThan:
            return left > right;
        default:
            return false;
    }
}

bool TransitionComparator::compareStrings(std::string left,
                                          std::string right,
                                          TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return left == right;
        case TransitionConditionOp::notEqual:
            return left != right;
        default:
            return false;
    }
}

bool TransitionComparator::compareBooleans(bool left, bool right, TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return left == right;
        case TransitionConditionOp::notEqual:
            return left != right;
        default:
            return false;
    }
}

bool TransitionComparator::compareEnums(uint16_t left, uint16_t right, TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return left == right;
        case TransitionConditionOp::notEqual:
            return left != right;
        default:
            return false;
    }
}

bool TransitionComparator::compareColors(int left, int right, TransitionConditionOp op)
{
    switch (op)
    {
        case TransitionConditionOp::equal:
            return left == right;
        case TransitionConditionOp::notEqual:
            return left != right;
        default:
            return false;
    }
}

bool TransitionComparator::compare(TransitionComparator* comparand,
                                   TransitionConditionOp operation,
                                   const StateMachineInstance* stateMachineInstance)
{
    return false;
}