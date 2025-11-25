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
    if (m_bindableProperty != nullptr)
    {
        delete m_bindableProperty;
        m_bindableProperty = nullptr;
    }
}

StatusCode TransitionPropertyViewModelComparator::import(
    ImportStack& importStack)
{
    auto bindablePropertyImporter =
        importStack.latest<BindablePropertyImporter>(
            BindablePropertyBase::typeKey);
    if (bindablePropertyImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    m_bindableProperty = bindablePropertyImporter->bindableProperty();

    return Super::import(importStack);
}

void TransitionPropertyViewModelComparator::useInLayer(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{

    auto bindableInstance =
        stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    if (bindableInstance == nullptr)
    {
        return;
    }
    auto dataBind =
        stateMachineInstance->bindableDataBindToTarget(bindableInstance);
    auto source = dataBind->source();
    if (source != nullptr)
    {
        source->useInLayer(layerInstance);
    }
}