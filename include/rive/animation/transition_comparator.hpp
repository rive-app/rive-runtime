#ifndef _RIVE_TRANSITION_COMPARATOR_HPP_
#define _RIVE_TRANSITION_COMPARATOR_HPP_
#include "rive/generated/animation/transition_comparator_base.hpp"
#include "rive/animation/transition_condition_op.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include <stdio.h>
namespace rive
{
class StateMachineInstance;
class StateMachineLayerInstance;
class TransitionComparator : public TransitionComparatorBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    virtual bool compare(TransitionComparator* comparand,
                         TransitionConditionOp operation,
                         const StateMachineInstance* stateMachineInstance,
                         StateMachineLayerInstance* layerInstance);

    virtual void useInLayer(const StateMachineInstance* stateMachineInstance,
                            StateMachineLayerInstance* layerInstance) const
    {}

protected:
    bool compareNumbers(float left, float right, TransitionConditionOp op);
    bool compareBooleans(bool left, bool right, TransitionConditionOp op);
    bool compareEnums(uint16_t left, uint16_t right, TransitionConditionOp op);
    bool compareColors(int left, int right, TransitionConditionOp op);
    bool compareStrings(std::string left,
                        std::string right,
                        TransitionConditionOp op);
    bool compareTriggers(uint32_t left,
                         uint32_t right,
                         TransitionConditionOp op);
};
} // namespace rive

#endif