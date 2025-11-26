#include "rive/data_bind/context/context_value_any.hpp"
#include "rive/generated/core_registry.hpp"
#include <cmath>

using namespace rive;

DataBindContextValueAny::DataBindContextValueAny(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueAny::apply(Core* target,
                                    uint32_t propertyKey,
                                    bool isMainDirection)
{
    syncSourceValue();
    auto dataValue =
        calculateUntypedDataValue(m_dataValue, isMainDirection, m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
            if (dataValue->is<DataValueNumber>())
            {
                CoreRegistry::setDouble(
                    target,
                    propertyKey,
                    dataValue->as<DataValueNumber>()->value());
            }
            break;
        case CoreUintType::id:
            if (dataValue->is<DataValueNumber>())
            {
                auto value = dataValue->as<DataValueNumber>()->value();
                if (target && target->is<Solo>())
                {
                    target->as<Solo>()->updateByIndex(
                        (size_t)std::round(value));
                }
                else
                {
                    int rounded = value < 0 ? 0 : std::round(value);
                    CoreRegistry::setUint(target, propertyKey, rounded);
                }
            }
            else if (dataValue->is<DataValueString>())
            {
                if (target && target->is<Solo>())
                {
                    target->as<Solo>()->updateByName(
                        dataValue->as<DataValueString>()->value());
                }
            }
            break;
        case CoreStringType::id:
            if (dataValue->is<DataValueString>())
            {
                CoreRegistry::setString(
                    target,
                    propertyKey,
                    dataValue->as<DataValueString>()->value());
            }
            break;
        case CoreBoolType::id:
            if (dataValue->is<DataValueBoolean>())
            {
                CoreRegistry::setBool(
                    target,
                    propertyKey,
                    dataValue->as<DataValueBoolean>()->value());
            }
            break;
        case CoreColorType::id:
            if (dataValue->is<DataValueColor>())
            {
                CoreRegistry::setColor(
                    target,
                    propertyKey,
                    dataValue->as<DataValueColor>()->value());
            }
            break;
    }
}