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
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
            if (m_dataValue->is<DataValueNumber>())
            {
                CoreRegistry::setDouble(
                    target,
                    propertyKey,
                    m_dataValue->as<DataValueNumber>()->value());
            }
            break;
        case CoreUintType::id:
            if (m_dataValue->is<DataValueNumber>())
            {
                auto value = m_dataValue->as<DataValueNumber>()->value();
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
            else if (m_dataValue->is<DataValueString>())
            {
                if (target && target->is<Solo>())
                {
                    target->as<Solo>()->updateByName(
                        m_dataValue->as<DataValueString>()->value());
                }
            }
            break;
        case CoreStringType::id:
            if (m_dataValue->is<DataValueString>())
            {
                CoreRegistry::setString(
                    target,
                    propertyKey,
                    m_dataValue->as<DataValueString>()->value());
            }
            break;
        case CoreBoolType::id:
            if (m_dataValue->is<DataValueBoolean>())
            {
                CoreRegistry::setBool(
                    target,
                    propertyKey,
                    m_dataValue->as<DataValueBoolean>()->value());
            }
            break;
        case CoreColorType::id:
            if (m_dataValue->is<DataValueColor>())
            {
                CoreRegistry::setColor(
                    target,
                    propertyKey,
                    m_dataValue->as<DataValueColor>()->value());
            }
            break;
    }
}