#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"

using namespace rive;

StatusCode TransitionPropertyViewModelComparator::import(ImportStack& importStack)
{
    auto bindablePropertyImporter =
        importStack.latest<BindablePropertyImporter>(BindablePropertyBase::typeKey);
    if (bindablePropertyImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    m_bindableProperty = bindablePropertyImporter->bindableProperty();

    return Super::import(importStack);
}

float TransitionPropertyViewModelComparator::propertyValueNumber(
    StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    return bindableInstance->as<BindablePropertyNumber>()->propertyValue();
}

std::string TransitionPropertyViewModelComparator::propertyValueString(
    StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    return bindableInstance->as<BindablePropertyString>()->propertyValue();
}

int TransitionPropertyViewModelComparator::propertyValueColor(
    StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    return bindableInstance->as<BindablePropertyColor>()->propertyValue();
}

bool TransitionPropertyViewModelComparator::propertyValueBoolean(
    StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    return bindableInstance->as<BindablePropertyBoolean>()->propertyValue();
}

uint16_t TransitionPropertyViewModelComparator::propertyValueEnum(
    StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    return bindableInstance->as<BindablePropertyEnum>()->propertyValue();
}

bool TransitionPropertyViewModelComparator::compare(TransitionComparator* comparand,
                                                    TransitionConditionOp operation,
                                                    StateMachineInstance* stateMachineInstance)
{
    switch (m_bindableProperty->coreType())
    {
        case BindablePropertyNumber::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()->propertyValueNumber(
                        stateMachineInstance);
                return compareNumbers(propertyValueNumber(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            else if (comparand->is<TransitionValueNumberComparator>())
            {
                auto rightValue = comparand->as<TransitionValueNumberComparator>()->value();
                return compareNumbers(propertyValueNumber(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            break;
        case BindablePropertyString::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()->propertyValueString(
                        stateMachineInstance);
                return compareStrings(propertyValueString(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            else if (comparand->is<TransitionValueStringComparator>())
            {
                auto rightValue = comparand->as<TransitionValueStringComparator>()->value();
                return compareStrings(propertyValueString(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            break;
        case BindablePropertyColor::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()->propertyValueColor(
                        stateMachineInstance);
                return compareColors(propertyValueColor(stateMachineInstance),
                                     rightValue,
                                     operation);
            }
            else if (comparand->is<TransitionValueColorComparator>())
            {
                auto rightValue = comparand->as<TransitionValueColorComparator>()->value();
                return compareColors(propertyValueColor(stateMachineInstance),
                                     rightValue,
                                     operation);
            }
            break;
        case BindablePropertyBoolean::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()->propertyValueBoolean(
                        stateMachineInstance);
                return compareBooleans(propertyValueBoolean(stateMachineInstance),
                                       rightValue,
                                       operation);
            }
            else if (comparand->is<TransitionValueBooleanComparator>())
            {
                auto rightValue = comparand->as<TransitionValueBooleanComparator>()->value();
                return compareBooleans(propertyValueBoolean(stateMachineInstance),
                                       rightValue,
                                       operation);
            }
            break;
        case BindablePropertyEnum::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()->propertyValueEnum(
                        stateMachineInstance);
                return compareEnums(propertyValueEnum(stateMachineInstance), rightValue, operation);
            }
            else if (comparand->is<TransitionValueEnumComparator>())
            {
                auto rightValue = comparand->as<TransitionValueEnumComparator>()->value();
                return compareEnums(propertyValueEnum(stateMachineInstance), rightValue, operation);
            }
            break;
    }
    return false;
}