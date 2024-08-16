#ifndef _RIVE_TRANSITION_PROPERTY_ARTBOARD_COMPARATOR_HPP_
#define _RIVE_TRANSITION_PROPERTY_ARTBOARD_COMPARATOR_HPP_
#include "rive/generated/animation/transition_property_artboard_comparator_base.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;
class TransitionPropertyArtboardComparator : public TransitionPropertyArtboardComparatorBase
{
public:
    bool compare(TransitionComparator* comparand,
                 TransitionConditionOp operation,
                 const StateMachineInstance* stateMachineInstance) override;

private:
    float propertyValue(const StateMachineInstance* stateMachineInstance);
};
} // namespace rive

#endif