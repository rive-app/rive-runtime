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
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_artboard.hpp"
#include "rive/data_bind/data_values/data_value_viewmodel.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

DataBindContextValue::DataBindContextValue(DataBind* dataBind)
{
    // Only allocate the target-side cache for binds that will read from the
    // target (toSource or TwoWay). Pure toTarget binds never touch
    // m_targetValue — calculateValueAndApply is only reached via
    // applyToSource(), which is guarded by toSource() in updateSourceBinding.
    if (dataBind->toSource())
    {
        m_targetValue.initialize(dataBind);
    }
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
                auto viewModelProperty =
                    viewmodelInstanceEnum->viewModelProperty();
                auto viewModelPropertyEnum =
                    viewModelProperty != nullptr &&
                            viewModelProperty->is<ViewModelPropertyEnum>()
                        ? viewModelProperty->as<ViewModelPropertyEnum>()
                        : nullptr;
                m_dataValue =
                    new DataValueEnum(viewmodelInstanceEnum->propertyValue(),
                                      viewModelPropertyEnum != nullptr
                                          ? viewModelPropertyEnum->dataEnum()
                                          : nullptr);
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
            case ViewModelInstanceAssetImageBase::typeKey:
                m_dataValue = new DataValueAssetImage(
                    source->as<ViewModelInstanceAssetImage>()->propertyValue());
                break;
            case ViewModelInstanceArtboardBase::typeKey:
                m_dataValue = new DataValueArtboard(
                    source->as<ViewModelInstanceArtboard>()->propertyValue());
                break;
            case ViewModelInstanceViewModelBase::typeKey:
                m_dataValue = new DataValueViewModel();
                break;
            default:
                m_dataValue = new DataValue();
        }
    }
}

void DataBindContextValue::syncSourceValue(DataBind* dataBind)
{
    auto source = dataBind->source();
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

            case ViewModelInstanceAssetImageBase::typeKey:
                m_dataValue->as<DataValueAssetImage>()->value(
                    source->as<ViewModelInstanceAssetImage>()->propertyValue());
                break;
            case ViewModelInstanceArtboardBase::typeKey:
                m_dataValue->as<DataValueArtboard>()->value(
                    source->as<ViewModelInstanceArtboard>()->propertyValue());
                break;
            case ViewModelInstanceViewModelBase::typeKey:
                m_dataValue->as<DataValueViewModel>()->value(
                    source->as<ViewModelInstanceViewModel>()
                        ->referenceViewModelInstance()
                        .get());
                break;
        }
    }
}

void DataBindContextValue::applyToSource(Core* component,
                                         uint32_t propertyKey,
                                         bool isMainDirection,
                                         DataBind* dataBind)
{
    auto source = dataBind->source();
    switch (source->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
        {

            calculateValueAndApply<DataValueNumber,
                                   float,
                                   ViewModelInstanceNumber>(isMainDirection,
                                                            dataBind);
        }
        break;
        case ViewModelInstanceStringBase::typeKey:
        {
            calculateValueAndApply<DataValueString,
                                   std::string,
                                   ViewModelInstanceString>(isMainDirection,
                                                            dataBind);
        }
        break;
        case ViewModelInstanceColorBase::typeKey:
        {
            calculateValueAndApply<DataValueColor, int, ViewModelInstanceColor>(
                isMainDirection,
                dataBind);
        }
        break;
        case ViewModelInstanceBooleanBase::typeKey:
        {
            calculateValueAndApply<DataValueBoolean,
                                   bool,
                                   ViewModelInstanceBoolean>(isMainDirection,
                                                             dataBind);
        }
        break;
        case ViewModelInstanceEnumBase::typeKey:
        {
            calculateValueAndApply<DataValueInteger,
                                   uint32_t,
                                   ViewModelInstanceEnum>(isMainDirection,
                                                          dataBind);
        }
        break;
        case ViewModelInstanceTriggerBase::typeKey:
        {
            calculateValueAndApply<DataValueInteger,
                                   uint32_t,
                                   ViewModelInstanceTrigger>(isMainDirection,
                                                             dataBind);
        }
        break;
        case ViewModelInstanceSymbolListIndexBase::typeKey:
        {
            calculateValueAndApply<DataValueInteger,
                                   uint32_t,
                                   ViewModelInstanceSymbolListIndex>(
                isMainDirection,
                dataBind);
        }
        break;
        case ViewModelInstanceAssetImageBase::typeKey:
        {
            calculateValueAndApply<DataValueInteger,
                                   uint32_t,
                                   ViewModelInstanceAssetImage>(isMainDirection,
                                                                dataBind);
        }
        break;
        case ViewModelInstanceArtboardBase::typeKey:
        {
            calculateValueAndApply<DataValueInteger,
                                   uint32_t,
                                   ViewModelInstanceArtboard>(isMainDirection,
                                                              dataBind);
        }
        break;
        case ViewModelInstanceViewModelBase::typeKey:
        {
            calculateValueAndApply<DataValueViewModel,
                                   ViewModelInstance*,
                                   ViewModelInstanceViewModel>(isMainDirection,
                                                               dataBind);
        }
        break;
    }
}