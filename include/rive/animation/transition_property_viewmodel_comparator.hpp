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
  template <typename T = BindableProperty, typename U>
  U value(const StateMachineInstance* stateMachineInstance)
  {
    if (m_bindableProperty != nullptr && m_bindableProperty->is<T>())
    {
      auto bindableInstance = stateMachineInstance->bindablePropertyInstance(
          m_bindableProperty);
      if (bindableInstance != nullptr)
      {
        return bindableInstance->as<T>()->propertyValue();
      }
    }
    return T::defaultValue;
  };
  void useInLayer(
      const StateMachineInstance* stateMachineInstance,
      StateMachineLayerInstance* layerInstance) const override;
  // Notes that the layer took a transition on this view model trigger's
  // fire, if one is pending for it. For a trigger on a condition's right,
  // which useInLayer isn't called for.
  void useTriggerFireInLayer(
      const StateMachineInstance* stateMachineInstance,
      StateMachineLayerInstance* layerInstance) const;
  BindableProperty* bindableProperty() { return m_bindableProperty; }
#ifdef WITH_RIVE_EDITOR
  void setBindablePropertyForEditor(BindableProperty* value)
  {
    m_editorBindable = true;
    m_bindableProperty = value;
  }
#endif

protected:
  // The view model value bound to this comparator's bindable, if any.
  ViewModelInstanceValue* source(
      const StateMachineInstance* stateMachineInstance) const;
  BindableProperty* m_bindableProperty = nullptr;
#ifdef WITH_RIVE_EDITOR
  bool m_editorBindable = false;
#endif
};
} // namespace rive

#endif
