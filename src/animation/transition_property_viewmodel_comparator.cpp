#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"
#include "rive/animation/transition_value_artboard_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/data_bind/bindable_property_artboard.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

TransitionPropertyViewModelComparator::~TransitionPropertyViewModelComparator()
{
#ifdef WITH_RIVE_EDITOR
  if (m_editorBindable)
    return;
#endif
  if (m_bindableProperty != nullptr)
  {
    delete m_bindableProperty;
    m_bindableProperty = nullptr;
  }
}

StatusCode TransitionPropertyViewModelComparator::import(
    ImportStack& importStack)
{
  auto bindablePropertyImporter = importStack.latest<BindablePropertyImporter>(
      BindablePropertyBase::typeKey);
  if (bindablePropertyImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  m_bindableProperty = bindablePropertyImporter->bindableProperty();

  return Super::import(importStack);
}

ViewModelInstanceValue* TransitionPropertyViewModelComparator::source(
    const StateMachineInstance* stateMachineInstance) const
{
  auto bindableInstance = stateMachineInstance->bindablePropertyInstance(
      m_bindableProperty);
  if (bindableInstance == nullptr)
  {
    return nullptr;
  }
  auto dataBind = stateMachineInstance->bindableDataBindToTarget(
      bindableInstance);
  return dataBind == nullptr ? nullptr : dataBind->source();
}

void TransitionPropertyViewModelComparator::useInLayer(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
  auto source = this->source(stateMachineInstance);
  if (source != nullptr)
  {
    stateMachineInstance->useViewModelValue(source, layerInstance);
  }
}

void TransitionPropertyViewModelComparator::useTriggerFireInLayer(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
  if (m_bindableProperty == nullptr ||
      !m_bindableProperty->is<BindablePropertyTrigger>())
  {
    return;
  }
  auto source = this->source(stateMachineInstance);
  // Only a fire the layer could read: noting a trigger that hasn't fired
  // would hide a fire later in the frame.
  if (source != nullptr &&
      stateMachineInstance->viewModelValueChanged(source, layerInstance))
  {
    stateMachineInstance->useViewModelValue(source, layerInstance);
  }
}
