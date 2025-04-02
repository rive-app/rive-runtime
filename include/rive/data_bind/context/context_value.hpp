#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContextValue
{
protected:
    DataBind* m_dataBind = nullptr;
    DataValue* m_dataValue = nullptr;
    bool m_isValid = false;
    virtual DataValue* targetValue() { return nullptr; };

public:
    DataBindContextValue(DataBind* dataBind);
    virtual ~DataBindContextValue()
    {
        if (m_dataValue != nullptr)
        {
            delete m_dataValue;
        }
    };
    virtual void applyToSource(Core* component,
                               uint32_t propertyKey,
                               bool isMainDirection);
    virtual void apply(Core* component,
                       uint32_t propertyKey,
                       bool isMainDirection){};
    virtual void update(Core* component){};
    void invalidate() { m_isValid = false; };
    virtual bool syncTargetValue(Core* target, uint32_t propertyKey)
    {
        return false;
    };
    void syncSourceValue();
    template <typename T = DataValue, typename U>
    U getDataValue(DataValue* input, DataBind* dataBind)
    {
        auto converter = dataBind->converter();
        auto dataValue =
            converter != nullptr ? converter->convert(input, dataBind) : input;
        if (dataValue->is<T>())
        {
            return dataValue->as<T>()->value();
        }
        return T::defaultValue;
    };
    template <typename T = DataValue, typename U>
    U getReverseDataValue(DataValue* input, DataBind* dataBind)
    {
        auto converter = dataBind->converter();
        auto dataValue = converter != nullptr
                             ? converter->reverseConvert(input, dataBind)
                             : input;
        if (dataValue->is<T>())
        {
            return dataValue->as<T>()->value();
        }
        return T::defaultValue;
    };
    template <typename T = DataValue, typename U>
    U calculateValue(DataValue* input, bool isMainDirection, DataBind* dataBind)
    {
        auto value = isMainDirection
                         ? getDataValue<T, U>(input, dataBind)
                         : getReverseDataValue<T, U>(input, dataBind);
        return value;
    };
    template <typename T = DataValue,
              typename U,
              typename V = ViewModelInstanceValue>
    void calculateValueAndApply(DataValue* input,
                                bool isMainDirection,
                                DataBind* dataBind,
                                Core* component,
                                uint32_t propertyKey)
    {
        // Check if target value changed or binding has been invalidated
        if (syncTargetValue(component, propertyKey) || !m_isValid)
        {
            // Calculate new value after converters are applied
            auto value = calculateValue<T, U>(input, isMainDirection, dataBind);
            // Apply value to source
            auto source = dataBind->source();
            source->as<V>()->propertyValue(value);
            m_isValid = true;
        }
    };
};
} // namespace rive

#endif