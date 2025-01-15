#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"

using namespace rive;

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

bool TransitionPropertyViewModelComparator::compare(
    TransitionComparator* comparand,
    TransitionConditionOp operation,
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance)
{
    switch (m_bindableProperty->coreType())
    {
        case BindablePropertyNumber::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyNumber, float>(
                            stateMachineInstance);
                return compareNumbers(
                    value<BindablePropertyNumber, float>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            else if (comparand->is<TransitionValueNumberComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueNumberComparator>()->value();
                return compareNumbers(
                    value<BindablePropertyNumber, float>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            break;
        case BindablePropertyString::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyString, std::string>(
                            stateMachineInstance);
                return compareStrings(
                    value<BindablePropertyString, std::string>(
                        stateMachineInstance),
                    rightValue,
                    operation);
            }
            else if (comparand->is<TransitionValueStringComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueStringComparator>()->value();
                return compareStrings(
                    value<BindablePropertyString, std::string>(
                        stateMachineInstance),
                    rightValue,
                    operation);
            }
            break;
        case BindablePropertyColor::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyColor, int>(
                            stateMachineInstance);
                return compareColors(
                    value<BindablePropertyColor, int>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            else if (comparand->is<TransitionValueColorComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueColorComparator>()->value();
                return compareColors(
                    value<BindablePropertyColor, int>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            break;
        case BindablePropertyBoolean::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyBoolean, bool>(
                            stateMachineInstance);
                return compareBooleans(
                    value<BindablePropertyBoolean, bool>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            else if (comparand->is<TransitionValueBooleanComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueBooleanComparator>()->value();
                return compareBooleans(
                    value<BindablePropertyBoolean, bool>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            break;
        case BindablePropertyEnum::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyEnum, uint16_t>(
                            stateMachineInstance);
                return compareEnums(
                    value<BindablePropertyEnum, uint16_t>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            else if (comparand->is<TransitionValueEnumComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueEnumComparator>()->value();
                return compareEnums(
                    value<BindablePropertyEnum, uint16_t>(stateMachineInstance),
                    rightValue,
                    operation);
            }
            break;
        case BindablePropertyTrigger::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyTrigger, uint32_t>(
                            stateMachineInstance);
                return compareTriggers(value<BindablePropertyTrigger, uint32_t>(
                                           stateMachineInstance),
                                       rightValue,
                                       operation);
            }
            else if (comparand->is<TransitionValueTriggerComparator>())
            {
                auto bindableInstance =
                    stateMachineInstance->bindablePropertyInstance(
                        m_bindableProperty);
                auto dataBind = stateMachineInstance->bindableDataBindToTarget(
                    bindableInstance);
                if (dataBind != nullptr)
                {
                    auto source = dataBind->source();
                    if (source != nullptr &&
                        source->is<ViewModelInstanceTrigger>())
                    {
                        if (!source->as<ViewModelInstanceTrigger>()->useInLayer(
                                layerInstance))
                        {

                            return false;
                        }
                    }
                }
                auto leftValue = value<BindablePropertyTrigger, uint32_t>(
                    stateMachineInstance);
                if (leftValue != 0)
                {
                    return true;
                }
            }
            break;
    }
    return false;
}