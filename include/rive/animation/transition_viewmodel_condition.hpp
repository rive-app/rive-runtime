#ifndef _RIVE_TRANSITION_VIEW_MODEL_CONDITION_HPP_
#define _RIVE_TRANSITION_VIEW_MODEL_CONDITION_HPP_
#include "rive/generated/animation/transition_viewmodel_condition_base.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_condition_op.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <stdio.h>
namespace rive
{
class TransitionViewModelCondition : public TransitionViewModelConditionBase
{
protected:
    TransitionComparator* m_leftComparator;
    TransitionComparator* m_rightComparator;

public:
    bool evaluate(const StateMachineInstance* stateMachineInstance) const override;
    TransitionComparator* leftComparator() const { return m_leftComparator; };
    TransitionComparator* rightComparator() const { return m_rightComparator; };
    void comparator(TransitionComparator* value)
    {
        if (m_leftComparator == nullptr)
        {
            m_leftComparator = value;
        }
        else
        {
            m_rightComparator = value;
        }
    };
    TransitionConditionOp op() const { return (TransitionConditionOp)opValue(); }
};
} // namespace rive

#endif