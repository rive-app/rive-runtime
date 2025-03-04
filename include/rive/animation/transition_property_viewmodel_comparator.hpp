#ifndef _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_HPP_
#define _RIVE_TRANSITION_PROPERTY_VIEW_MODEL_COMPARATOR_HPP_
#include "rive/generated/animation/transition_property_viewmodel_comparator_base.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include <stdio.h>
namespace rive
{
class TransitionPropertyViewModelComparator
    : public TransitionPropertyViewModelComparatorBase
{
public:
    ~TransitionPropertyViewModelComparator();
    StatusCode import(ImportStack& importStack) override;
    bool compare(TransitionComparator* comparand,
                 TransitionConditionOp operation,
                 const StateMachineInstance* stateMachineInstance,
                 StateMachineLayerInstance* layerInstance) override;
    template <typename T = BindableProperty, typename U>
    U value(const StateMachineInstance* stateMachineInstance)
    {
        if (m_bindableProperty != nullptr && m_bindableProperty->is<T>())
        {
            auto bindableInstance =
                stateMachineInstance->bindablePropertyInstance(
                    m_bindableProperty);
            if (bindableInstance != nullptr)
            {
                return bindableInstance->as<T>()->propertyValue();
            }
        }
        return T::defaultValue;
    };
    void useInLayer(const StateMachineInstance* stateMachineInstance,
                    StateMachineLayerInstance* layerInstance) const override;

protected:
    BindableProperty* m_bindableProperty;
};
} // namespace rive

#endif