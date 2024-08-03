#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValue::DataBindContextValue(ViewModelInstanceValue* source,
                                           DataConverter* converter) :
    m_source(source), m_converter(converter)
{
    if (m_source != nullptr)
    {
        switch (m_source->coreType())
        {
            case ViewModelInstanceNumberBase::typeKey:
                m_dataValue =
                    new DataValueNumber(m_source->as<ViewModelInstanceNumber>()->propertyValue());
                break;
            case ViewModelInstanceStringBase::typeKey:
                m_dataValue =
                    new DataValueString(m_source->as<ViewModelInstanceString>()->propertyValue());
                break;
            case ViewModelInstanceColorBase::typeKey:
                m_dataValue =
                    new DataValueColor(m_source->as<ViewModelInstanceColor>()->propertyValue());
                break;
            case ViewModelInstanceBooleanBase::typeKey:
                m_dataValue =
                    new DataValueBoolean(m_source->as<ViewModelInstanceBoolean>()->propertyValue());
                break;
            case ViewModelInstanceEnumBase::typeKey:
            {
                auto viewmodelInstanceEnum = m_source->as<ViewModelInstanceEnum>();
                auto viewModelPropertyEnum =
                    viewmodelInstanceEnum->viewModelProperty()->as<ViewModelPropertyEnum>();
                m_dataValue = new DataValueEnum(viewmodelInstanceEnum->propertyValue(),
                                                viewModelPropertyEnum->dataEnum());
            }
            break;
            default:
                m_dataValue = new DataValue();
        }
    }
}

void DataBindContextValue::updateSourceValue()
{
    if (m_source != nullptr)
    {
        switch (m_source->coreType())
        {
            case ViewModelInstanceNumberBase::typeKey:
                m_dataValue->as<DataValueNumber>()->value(
                    m_source->as<ViewModelInstanceNumber>()->propertyValue());
                break;
            case ViewModelInstanceStringBase::typeKey:
                m_dataValue->as<DataValueString>()->value(
                    m_source->as<ViewModelInstanceString>()->propertyValue());
                break;
            case ViewModelInstanceColorBase::typeKey:
                m_dataValue->as<DataValueColor>()->value(
                    m_source->as<ViewModelInstanceColor>()->propertyValue());
                break;
            case ViewModelInstanceBooleanBase::typeKey:
                m_dataValue->as<DataValueBoolean>()->value(
                    m_source->as<ViewModelInstanceBoolean>()->propertyValue());
                break;
            case ViewModelInstanceEnumBase::typeKey:
                m_dataValue->as<DataValueEnum>()->value(
                    m_source->as<ViewModelInstanceEnum>()->propertyValue());
                break;
        }
    }
}

void DataBindContextValue::applyToSource(Core* component,
                                         uint32_t propertyKey,
                                         bool isMainDirection)
{
    auto targetValue = getTargetValue(component, propertyKey);
    switch (m_source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
        {

            auto value = calculateValue<DataValueNumber, float>(targetValue, isMainDirection);
            m_source->as<ViewModelInstanceNumber>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceStringBase::typeKey:
        {
            auto value = calculateValue<DataValueString, std::string>(targetValue, isMainDirection);
            m_source->as<ViewModelInstanceString>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceColorBase::typeKey:
        {
            auto value = calculateValue<DataValueColor, int>(targetValue, isMainDirection);
            m_source->as<ViewModelInstanceColor>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceBooleanBase::typeKey:
        {
            auto value = calculateValue<DataValueBoolean, bool>(targetValue, isMainDirection);
            m_source->as<ViewModelInstanceBoolean>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceEnumBase::typeKey:
        {
            auto value = calculateValue<DataValueEnum, uint32_t>(targetValue, isMainDirection);
            m_source->as<ViewModelInstanceEnum>()->propertyValue(value);
        }
        break;
    }
}