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

    virtual void useInLayer(const StateMachineInstance* stateMachineInstance,
                            StateMachineLayerInstance* layerInstance) const {};
};
} // namespace rive
#endif