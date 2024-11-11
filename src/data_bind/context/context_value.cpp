#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValue::DataBindContextValue(DataBind* dataBind) :
    m_dataBind(dataBind)
{
    auto source = dataBind->source();
    if (source != nullptr)
    {
        switch (source->coreType())
        {
            case ViewModelInstanceNumberBase::typeKey:
                m_dataValue = new DataValueNumber(
                    source->as<ViewModelInstanceNumber>()->propertyValue());
                break;
            case ViewModelInstanceStringBase::typeKey:
                m_dataValue = new DataValueString(
                    source->as<ViewModelInstanceString>()->propertyValue());
                break;
            case ViewModelInstanceColorBase::typeKey:
                m_dataValue = new DataValueColor(
                    source->as<ViewModelInstanceColor>()->propertyValue());
                break;
            case ViewModelInstanceBooleanBase::typeKey:
                m_dataValue = new DataValueBoolean(
                    source->as<ViewModelInstanceBoolean>()->propertyValue());
                break;
            case ViewModelInstanceEnumBase::typeKey:
            {
                auto viewmodelInstanceEnum =
                    source->as<ViewModelInstanceEnum>();
                auto viewModelPropertyEnum =
                    viewmodelInstanceEnum->viewModelProperty()
                        ->as<ViewModelPropertyEnum>();
                m_dataValue =
                    new DataValueEnum(viewmodelInstanceEnum->propertyValue(),
                                      viewModelPropertyEnum->dataEnum());
            }
            break;
            case ViewModelInstanceTriggerBase::typeKey:
                m_dataValue = new DataValueTrigger(
                    source->as<ViewModelInstanceTrigger>()->propertyValue());
                break;
            default:
                m_dataValue = new DataValue();
        }
    }
}

void DataBindContextValue::updateSourceValue()
{
    auto source = m_dataBind->source();
    if (source != nullptr)
    {
        switch (source->coreType())
        {
            case ViewModelInstanceNumberBase::typeKey:
                m_dataValue->as<DataValueNumber>()->value(
                    source->as<ViewModelInstanceNumber>()->propertyValue());
                break;
            case ViewModelInstanceStringBase::typeKey:
                m_dataValue->as<DataValueString>()->value(
                    source->as<ViewModelInstanceString>()->propertyValue());
                break;
            case ViewModelInstanceColorBase::typeKey:
                m_dataValue->as<DataValueColor>()->value(
                    source->as<ViewModelInstanceColor>()->propertyValue());
                break;
            case ViewModelInstanceBooleanBase::typeKey:
                m_dataValue->as<DataValueBoolean>()->value(
                    source->as<ViewModelInstanceBoolean>()->propertyValue());
                break;
            case ViewModelInstanceEnumBase::typeKey:
                m_dataValue->as<DataValueEnum>()->value(
                    source->as<ViewModelInstanceEnum>()->propertyValue());
                break;
            case ViewModelInstanceTriggerBase::typeKey:
                m_dataValue->as<DataValueTrigger>()->value(
                    source->as<ViewModelInstanceTrigger>()->propertyValue());
                break;
        }
    }
}

void DataBindContextValue::applyToSource(Core* component,
                                         uint32_t propertyKey,
                                         bool isMainDirection)
{
    auto source = m_dataBind->source();
    auto targetValue = getTargetValue(component, propertyKey);
    switch (source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
        {

            auto value = calculateValue<DataValueNumber, float>(targetValue,
                                                                isMainDirection,
                                                                m_dataBind);
            source->as<ViewModelInstanceNumber>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceStringBase::typeKey:
        {
            auto value =
                calculateValue<DataValueString, std::string>(targetValue,
                                                             isMainDirection,
                                                             m_dataBind);
            source->as<ViewModelInstanceString>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceColorBase::typeKey:
        {
            auto value = calculateValue<DataValueColor, int>(targetValue,
                                                             isMainDirection,
                                                             m_dataBind);
            source->as<ViewModelInstanceColor>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceBooleanBase::typeKey:
        {
            auto value = calculateValue<DataValueBoolean, bool>(targetValue,
                                                                isMainDirection,
                                                                m_dataBind);
            source->as<ViewModelInstanceBoolean>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceEnumBase::typeKey:
        {
            auto value =
                calculateValue<DataValueEnum, uint32_t>(targetValue,
                                                        isMainDirection,
                                                        m_dataBind);
            source->as<ViewModelInstanceEnum>()->propertyValue(value);
        }
        break;
        case ViewModelInstanceTriggerBase::typeKey:
        {
            auto value =
                calculateValue<DataValueTrigger, uint32_t>(targetValue,
                                                           isMainDirection,
                                                           m_dataBind);
            source->as<ViewModelInstanceTrigger>()->propertyValue(value);
        }
        break;
    }
}

// template <typename T, typename U>
// U DataBindContextValue::getDataValue(DataValue* input, DataBind* dataBind)
// {
//     auto converter = dataBind->converter();
//     auto dataValue =
//         converter != nullptr ? converter->convert(input, dataBind) : input;
//     if (dataValue->is<T>())
//     {
//         return dataValue->as<T>()->value();
//     }
//     return T::defaultValue;
// };

// template <typename T, typename U>
// U DataBindContextValue::getReverseDataValue(DataValue* input,
//                                             DataBind* dataBind)
// {
//     auto converter = dataBind->converter();
//     auto dataValue = converter != nullptr
//                          ? converter->reverseConvert(input, dataBind)
//                          : input;
//     if (dataValue->is<T>())
//     {
//         return dataValue->as<T>()->value();
//     }
//     return T::defaultValue;
// };

// template <typename T, typename U>
// U DataBindContextValue::calculateValue(DataValue* input,
//                                        bool isMainDirection,
//                                        DataBind* dataBind)
// {
//     return isMainDirection ? getDataValue<T, U>(input, dataBind)
//                            : getReverseDataValue<T, U>(input, dataBind);
// };