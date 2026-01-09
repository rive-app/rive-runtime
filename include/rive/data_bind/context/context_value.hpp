#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/context/context_target_value.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContextValue
{
protected:
    DataBind* m_dataBind = nullptr;
    DataValue* m_dataValue = nullptr;
    DataBindContextTargetValue m_targetValue;
    bool m_isValid = false;

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
                       bool isMainDirection) {};
    void invalidate() { m_isValid = false; };
    virtual bool syncTargetValue(Core* target, uint32_t propertyKey)
    {
        return false;
    };
    void syncSourceValue();
    DataValue* calculateUntypedDataValue(DataValue* input,
                                         bool isMainDirection,
                                         DataBind* dataBind)
    {
        auto converter = dataBind->converter();
        auto dataValue = converter != nullptr
                             ? isMainDirection
                                   ? converter->convert(input, dataBind)
                                   : converter->reverseConvert(input, dataBind)
                             : input;
        return dataValue;
    }
    template <typename T = DataValue>
    DataValue* calculateDataValue(DataValue* input,
                                  bool isMainDirection,
                                  DataBind* dataBind)
    {
        auto dataValue =
            calculateUntypedDataValue(input, isMainDirection, dataBind);
        if (dataValue->is<T>())
        {
            return dataValue;
        }
        return nullptr;
    };

    template <typename T = DataValue, typename U>
    U calculateValue(DataValue* input, bool isMainDirection, DataBind* dataBind)
    {
        auto dataValue =
            calculateDataValue<T>(input, isMainDirection, dataBind);
        if (dataValue && dataValue->template is<T>())
        {
            return dataValue->template as<T>()->value();
        }
        return T::defaultValue;
    };
    template <typename T = DataValue,
              typename U,
              typename V = ViewModelInstanceValue>
    void calculateValueAndApply(bool isMainDirection)
    {
        // Check if target value changed or binding has been invalidated
        if (m_targetValue.syncTargetValue() || !m_isValid)
        {
            // Calculate new value after converters are applied
            auto value = calculateDataValue<T>(m_targetValue.dataValue(),
                                               isMainDirection,
                                               m_dataBind);
            if (value)
            {

                // Apply value to source
                m_dataBind->suppressDirt(true);
                auto source = m_dataBind->source();
                source->as<V>()->applyValue(value->template as<T>());
                m_dataBind->suppressDirt(false);
                m_isValid = true;
            }
        }
    };
};
} // namespace rive

#endif