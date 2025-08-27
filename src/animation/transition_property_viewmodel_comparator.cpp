#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/bindable_property_importer.hpp"
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

float TransitionPropertyViewModelComparator::valueToFloat(
    const StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance =
        stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    if (bindableInstance != nullptr)
    {
        if (bindableInstance->is<BindablePropertyInteger>())
        {
            return (float)this->value<BindablePropertyInteger, uint32_t>(
                stateMachineInstance);
        }
        else if (bindableInstance->is<BindablePropertyNumber>())
        {
            return this->value<BindablePropertyNumber, float>(
                stateMachineInstance);
        }
    }
    return 0;
}

bool TransitionPropertyViewModelComparator::compare(
    TransitionComparator* comparand,
    TransitionConditionOp operation,
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance)
{
    if (comparand->is<TransitionSelfComparator>())
    {
        auto bindableInstance =
            stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
        auto dataBind =
            stateMachineInstance->bindableDataBindToTarget(bindableInstance);
        if (dataBind != nullptr)
        {
            auto source = dataBind->source();
            if (source != nullptr && source->hasChanged())
            {
                if (source->isUsedInLayer(layerInstance))
                {

                    return false;
                }
                return true;
            }
        }
        return false;
    }
    switch (m_bindableProperty->coreType())
    {
        case BindablePropertyNumber::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->valueToFloat(stateMachineInstance);
                return compareNumbers(valueToFloat(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            else if (comparand->is<TransitionValueNumberComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueNumberComparator>()->value();
                return compareNumbers(valueToFloat(stateMachineInstance),
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
                        if (source->as<ViewModelInstanceTrigger>()
                                ->isUsedInLayer(layerInstance))
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
        case BindablePropertyInteger::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->valueToFloat(stateMachineInstance);
                return compareNumbers(valueToFloat(stateMachineInstance),
                                      rightValue,
                                      operation);
            }
            else if (comparand->is<TransitionValueNumberComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueNumberComparator>()->value();
                switch (instanceDataType(stateMachineInstance))
                {
                    case DataType::number:
                        return compareNumbers(
                            valueToFloat(stateMachineInstance),
                            rightValue,
                            operation);
                    case DataType::integer:
                    {

                        auto val = value<BindablePropertyInteger, uint32_t>(
                            stateMachineInstance);
                        return compareNumbers((float)val,
                                              rightValue,
                                              operation);
                    }
                    default:
                        break;
                }
            }
            break;
        case BindablePropertyAsset::typeKey:
            if (comparand->is<TransitionPropertyViewModelComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionPropertyViewModelComparator>()
                        ->value<BindablePropertyAsset, uint32_t>(
                            stateMachineInstance);
                return compareIds(value<BindablePropertyAsset, uint32_t>(
                                      stateMachineInstance),
                                  rightValue,
                                  operation);
            }
            else if (comparand->is<TransitionValueAssetComparator>())
            {
                auto rightValue =
                    comparand->as<TransitionValueAssetComparator>()->value();
                return compareIds(value<BindablePropertyAsset, uint32_t>(
                                      stateMachineInstance),
                                  rightValue,
                                  operation);
            }
            break;
    }
    return false;
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

DataType TransitionPropertyViewModelComparator::instanceDataType(
    const StateMachineInstance* stateMachineInstance)
{
    auto bindableInstance =
        stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    if (bindableInstance != nullptr)
    {

        switch (bindableInstance->coreType())
        {
            case BindablePropertyNumberBase::typeKey:

                return DataType::number;
            case BindablePropertyBooleanBase::typeKey:
                return DataType::boolean;
            case BindablePropertyColorBase::typeKey:
                return DataType::color;
            case BindablePropertyStringBase::typeKey:
                return DataType::string;
            case BindablePropertyEnumBase::typeKey:
                return DataType::enumType;
            case BindablePropertyTriggerBase::typeKey:
                return DataType::trigger;
            case BindablePropertyIntegerBase::typeKey:
                return DataType::integer;
        }
    }
    return DataType::none;
}