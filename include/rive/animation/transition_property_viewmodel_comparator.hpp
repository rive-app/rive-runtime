#ifndef _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_HPP_
#define _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_HPP_
#include "rive/generated/animation/transition_property_viewmodel_comparator_base.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include <stdio.h>
namespace rive
{
class TransitionPropertyViewModelComparator : public TransitionPropertyViewModelComparatorBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    bool compare(TransitionComparator* comparand,
                 TransitionConditionOp operation,
                 StateMachineInstance* stateMachineInstance) override;
    float propertyValueNumber(StateMachineInstance* stateMachineInstance);
    std::string propertyValueString(StateMachineInstance* stateMachineInstance);
    int propertyValueColor(StateMachineInstance* stateMachineInstance);
    bool propertyValueBoolean(StateMachineInstance* stateMachineInstance);
    uint16_t propertyValueEnum(StateMachineInstance* stateMachineInstance);

protected:
    BindableProperty* m_bindableProperty;
};
} // namespace rive

#endif