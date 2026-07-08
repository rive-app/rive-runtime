#include "rive/data_bind/context/context_target_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_asset_font.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_viewmodel.hpp"
#include "rive/data_bind/bindable_property_viewmodel.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextTargetValue::~DataBindContextTargetValue()
{
    if (m_targetValue != nullptr)
    {
        delete m_targetValue;
    }
};

void DataBindContextTargetValue::initialize(DataBind* dataBind)
{
    switch (CoreRegistry::propertyFieldId(dataBind->propertyKey()))
    {
        case CoreUintType::id:
        {
            // Solos are a special case where the output type is relevant to
            // apply the reverse conversion. In the furute we might need to
            // traverse the full conversion chain to make sure we create the
            // right type.
            if (dataBind->propertyKey() ==
                SoloBase::activeComponentIdPropertyKey)
            {
                if (dataBind->sourceOutputType() == DataType::string)
                {
                    m_targetValue = new DataValueString();
                }
                else if (dataBind->sourceOutputType() == DataType::number)
                {
                    m_targetValue = new DataValueNumber();
                }
                else if (dataBind->sourceOutputType() == DataType::enumType)
                {
                    m_targetValue = new DataValueInteger();
                }
            }
            else if (dataBind->source() != nullptr &&
                     dataBind->source()->coreType() ==
                         ViewModelInstanceAssetImageBase::typeKey)
            {
                m_targetValue = new DataValueAssetImage();
            }
            else if (dataBind->source() != nullptr &&
                     dataBind->source()->coreType() ==
                         ViewModelInstanceAssetFontBase::typeKey)
            {
                m_targetValue = new DataValueAssetFont();
            }
            else if (dataBind->source() != nullptr &&
                     dataBind->source()->coreType() ==
                         ViewModelInstanceViewModelBase::typeKey)
            {
                m_targetValue = new DataValueViewModel();
            }
            else
            {
                m_targetValue = new DataValueInteger();
            }
        }
        break;
        case CoreColorType::id:
        {
            m_targetValue = new DataValueColor();
        }
        break;
        case CoreDoubleType::id:
        {
            m_targetValue = new DataValueNumber();
        }
        break;
        case CoreStringType::id:
        {
            m_targetValue = new DataValueString();
        }
        break;
        case CoreBoolType::id:
        {
            m_targetValue = new DataValueBoolean();
        }
        break;
        default:
            break;
    }
}

bool DataBindContextTargetValue::syncTargetValue(DataBind* dataBind)
{
    if (!dataBind->target())
    {
        return false;
    }
    switch (CoreRegistry::propertyFieldId(dataBind->propertyKey()))
    {
        case CoreUintType::id:
        {
            if (dataBind->propertyKey() ==
                    SoloBase::activeComponentIdPropertyKey &&
                dataBind->target()->is<Solo>())
            {
                auto target = dataBind->target()->as<Solo>();
                if (dataBind->sourceOutputType() == DataType::string)
                {
                    auto value = target->getActiveChildName();
                    return updateValue<DataValueString, std::string>(value);
                }
                else if (dataBind->sourceOutputType() == DataType::number)
                {
                    auto value = (float)target->getActiveChildIndex();
                    return updateValue<DataValueNumber, float>(value);
                }
                else if (dataBind->sourceOutputType() == DataType::integer)
                {
                    auto value = target->getActiveChildIndex();
                    return updateValue<DataValueInteger, int>(value);
                }
                else if (dataBind->sourceOutputType() == DataType::enumType)
                {
                    auto activeChildName = target->getActiveChildName();
                    if (dataBind->source() &&
                        dataBind->source()->is<ViewModelInstanceEnum>())
                    {
                        auto viewModelInstanceEnum =
                            dataBind->source()->as<ViewModelInstanceEnum>();
                        if (viewModelInstanceEnum->viewModelProperty())
                        {
                            auto enumProperty =
                                viewModelInstanceEnum->viewModelProperty()
                                    ->as<ViewModelPropertyEnum>();
                            if (enumProperty && enumProperty->dataEnum())
                            {
                                auto dataEnum = enumProperty->dataEnum();
                                auto index =
                                    dataEnum->valueIndex(activeChildName);
                                return updateValue<DataValueInteger, int>(
                                    index);
                            }
                        }
                    }
                }
            }
            else if (dataBind->target()->coreType() ==
                     BindablePropertyAssetBase::typeKey)
            {
                auto bindableAsset =
                    dataBind->target()->as<BindablePropertyAsset>();
                auto value = CoreRegistry::getUint(dataBind->target(),
                                                   dataBind->propertyKey());
                bool didChange = false;
                if (updateValue<DataValueInteger, int>(value))
                {
                    didChange = true;
                }
                // BindablePropertyAsset carries either a live image or a live
                // font; sync whichever matches the target value's kind (the id
                // above is always synced).
                if (m_targetValue->is<DataValueAssetImage>())
                {
                    auto image = bindableAsset->fileAsset()->renderImage();
                    if (image !=
                        m_targetValue->as<DataValueAssetImage>()->imageValue())
                    {
                        m_targetValue->as<DataValueAssetImage>()->imageValue(
                            image);
                        didChange = true;
                    }
                }
                else if (m_targetValue->is<DataValueAssetFont>())
                {
                    auto font = bindableAsset->fontValue();
                    if (font !=
                        m_targetValue->as<DataValueAssetFont>()->fontValue())
                    {
                        m_targetValue->as<DataValueAssetFont>()->fontValue(
                            font);
                        didChange = true;
                    }
                }
                return didChange;
            }
            else if (dataBind->target()->coreType() ==
                     BindablePropertyViewModelBase::typeKey)
            {
                bool didChange = false;
                auto viewModelInstance = dataBind->target()
                                             ->as<BindablePropertyViewModel>()
                                             ->viewModelInstanceValue();
                if (m_targetValue->is<DataValueViewModel>() &&
                    m_targetValue->as<DataValueViewModel>()->value() !=
                        viewModelInstance)
                {
                    m_targetValue->as<DataValueViewModel>()->value(
                        viewModelInstance);
                    didChange = true;
                }
                return didChange;
            }
            else if (dataBind->target()->coreType() ==
                     ViewModelInstanceViewModelBase::typeKey)
            {
                bool didChange = false;
                auto viewModelInstanceViewModel =
                    dataBind->target()->as<ViewModelInstanceViewModel>();
                if (viewModelInstanceViewModel->referenceViewModelInstance()
                        .get() !=
                    m_targetValue->as<DataValueViewModel>()->value())
                {
                    m_targetValue->as<DataValueViewModel>()->value(
                        viewModelInstanceViewModel->referenceViewModelInstance()
                            .get());
                    didChange = true;
                }
                return didChange;
            }
            else
            {
                auto value = CoreRegistry::getUint(dataBind->target(),
                                                   dataBind->propertyKey());
                return updateValue<DataValueInteger, int>(value);
            }
        }
        break;
        case CoreColorType::id:
        {
            auto value = CoreRegistry::getColor(dataBind->target(),
                                                dataBind->propertyKey());
            return updateValue<DataValueColor, int>(value);
        }
        break;
        case CoreDoubleType::id:
        {
            auto value = CoreRegistry::getDouble(dataBind->target(),
                                                 dataBind->propertyKey());
            return updateValue<DataValueNumber, float>(value);
        }
        break;
        case CoreStringType::id:
        {
            auto value = CoreRegistry::getString(dataBind->target(),
                                                 dataBind->propertyKey());
            return updateValue<DataValueString, std::string>(value);
        }
        break;
        case CoreBoolType::id:
        {
            auto value = CoreRegistry::getBool(dataBind->target(),
                                               dataBind->propertyKey());
            return updateValue<DataValueBoolean, bool>(value);
        }
        break;
        default:
            break;
    }
    return false;
}
