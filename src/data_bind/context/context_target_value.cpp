#include "rive/data_bind/context/context_target_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
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
    m_dataBind = dataBind;
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
            }
            else if (dataBind->source()->coreType() ==
                     ViewModelInstanceAssetImageBase::typeKey)
            {
                m_targetValue = new DataValueAssetImage();
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

bool DataBindContextTargetValue::syncTargetValue()
{
    switch (CoreRegistry::propertyFieldId(m_dataBind->propertyKey()))
    {
        case CoreUintType::id:
        {
            if (m_dataBind->propertyKey() ==
                    SoloBase::activeComponentIdPropertyKey &&
                m_dataBind->target() && m_dataBind->target()->is<Solo>())
            {
                auto target = m_dataBind->target()->as<Solo>();
                if (m_dataBind->sourceOutputType() == DataType::string)
                {
                    auto value = target->getActiveChildName();
                    return updateValue<DataValueString, std::string>(value);
                }
                else if (m_dataBind->sourceOutputType() == DataType::number)
                {
                    auto value = (float)target->getActiveChildIndex();
                    return updateValue<DataValueNumber, float>(value);
                }
            }
            else if (m_dataBind->target()->coreType() ==
                     BindablePropertyAssetBase::typeKey)
            {
                auto value = CoreRegistry::getUint(m_dataBind->target(),
                                                   m_dataBind->propertyKey());
                auto fileAsset = m_dataBind->target()
                                     ->as<BindablePropertyAsset>()
                                     ->fileAsset();
                bool didChange = false;
                if (updateValue<DataValueInteger, int>(value))
                {
                    didChange = true;
                }
                if (fileAsset->renderImage() !=
                    m_targetValue->as<DataValueAssetImage>()->imageValue())
                {
                    m_targetValue->as<DataValueAssetImage>()->imageValue(
                        fileAsset->renderImage());
                    didChange = true;
                }
                return didChange;
            }
            else
            {
                auto value = CoreRegistry::getUint(m_dataBind->target(),
                                                   m_dataBind->propertyKey());
                return updateValue<DataValueInteger, int>(value);
            }
        }
        break;
        case CoreColorType::id:
        {
            auto value = CoreRegistry::getColor(m_dataBind->target(),
                                                m_dataBind->propertyKey());
            return updateValue<DataValueColor, int>(value);
        }
        break;
        case CoreDoubleType::id:
        {
            auto value = CoreRegistry::getDouble(m_dataBind->target(),
                                                 m_dataBind->propertyKey());
            return updateValue<DataValueNumber, float>(value);
        }
        break;
        case CoreStringType::id:
        {
            auto value = CoreRegistry::getString(m_dataBind->target(),
                                                 m_dataBind->propertyKey());
            return updateValue<DataValueString, std::string>(value);
        }
        break;
        case CoreBoolType::id:
        {
            auto value = CoreRegistry::getBool(m_dataBind->target(),
                                               m_dataBind->propertyKey());
            return updateValue<DataValueBoolean, bool>(value);
        }
        break;
        default:
            break;
    }
    return false;
}
