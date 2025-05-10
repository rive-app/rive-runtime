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
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
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
            case ViewModelInstanceListBase::typeKey:
                m_dataValue = new DataValueList();
                break;
            case ViewModelInstanceSymbolListIndexBase::typeKey:
                m_dataValue = new DataValueSymbolListIndex();
                break;
            default:
                m_dataValue = new DataValue();
        }
    }
}

void DataBindContextValue::syncSourceValue()
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
            case ViewModelInstanceListBase::typeKey:
            {
                m_dataValue->as<DataValueList>()->clear();
                auto items = source->as<ViewModelInstanceList>()->listItems();
                for (auto& item : items)
                {
                    m_dataValue->as<DataValueList>()->addItem(item);
                }
                break;
            }
            case ViewModelInstanceSymbolListIndexBase::typeKey:
                m_dataValue->as<DataValueSymbolListIndex>()->value(
                    source->as<ViewModelInstanceSymbolListIndex>()
                        ->propertyValue());
                break;
        }
    }
}

void DataBindContextValue::applyToSource(Core* component,
                                         uint32_t propertyKey,
                                         bool isMainDirection)
{
    auto source = m_dataBind->source();
    switch (source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
        {

            calculateValueAndApply<DataValueNumber,
                                   float,
                                   ViewModelInstanceNumber>(targetValue(),
                                                            isMainDirection,
                                                            m_dataBind,
                                                            component,
                                                            propertyKey);
        }
        break;
        case ViewModelInstanceStringBase::typeKey:
        {
            calculateValueAndApply<DataValueString,
                                   std::string,
                                   ViewModelInstanceString>(targetValue(),
                                                            isMainDirection,
                                                            m_dataBind,
                                                            component,
                                                            propertyKey);
        }
        break;
        case ViewModelInstanceColorBase::typeKey:
        {
            calculateValueAndApply<DataValueColor, int, ViewModelInstanceColor>(
                targetValue(),
                isMainDirection,
                m_dataBind,
                component,
                propertyKey);
        }
        break;
        case ViewModelInstanceBooleanBase::typeKey:
        {
            calculateValueAndApply<DataValueBoolean,
                                   bool,
                                   ViewModelInstanceBoolean>(targetValue(),
                                                             isMainDirection,
                                                             m_dataBind,
                                                             component,
                                                             propertyKey);
        }
        break;
        case ViewModelInstanceEnumBase::typeKey:
        {
            calculateValueAndApply<DataValueEnum,
                                   uint32_t,
                                   ViewModelInstanceEnum>(targetValue(),
                                                          isMainDirection,
                                                          m_dataBind,
                                                          component,
                                                          propertyKey);
        }
        break;
        case ViewModelInstanceTriggerBase::typeKey:
        {
            calculateValueAndApply<DataValueTrigger,
                                   uint32_t,
                                   ViewModelInstanceTrigger>(targetValue(),
                                                             isMainDirection,
                                                             m_dataBind,
                                                             component,
                                                             propertyKey);
        }
        break;
        case ViewModelInstanceSymbolListIndexBase::typeKey:
        {
            calculateValueAndApply<DataValueTrigger,
                                   uint32_t,
                                   ViewModelInstanceSymbolListIndex>(
                targetValue(),
                isMainDirection,
                m_dataBind,
                component,
                propertyKey);
        }
        break;
    }
}