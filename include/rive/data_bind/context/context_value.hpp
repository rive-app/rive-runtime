#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContextValue
{
protected:
    ViewModelInstanceValue* m_source;
    DataConverter* m_converter;
    DataValue* m_dataValue;

public:
    DataBindContextValue(ViewModelInstanceValue* source, DataConverter* converter);
    virtual ~DataBindContextValue(){};
    virtual void applyToSource(Core* component, uint32_t propertyKey, bool isMainDirection);
    virtual void apply(Core* component, uint32_t propertyKey, bool isMainDirection){};
    virtual void update(Core* component){};
    virtual DataValue* getTargetValue(Core* target, uint32_t propertyKey) { return nullptr; };
    void updateSourceValue();
    template <typename T = DataValue, typename U> U getDataValue(DataValue* input)
    {
        auto dataValue = m_converter != nullptr ? m_converter->convert(input) : input;
        if (dataValue->is<T>())
        {
            return dataValue->as<T>()->value();
        }
        return (new T())->value();
    };
    template <typename T = DataValue, typename U> U getReverseDataValue(DataValue* input)
    {
        auto dataValue = m_converter != nullptr ? m_converter->reverseConvert(input) : input;
        if (dataValue->is<T>())
        {
            return dataValue->as<T>()->value();
        }
        return (new T())->value();
    };
    template <typename T = DataValue, typename U>
    U calculateValue(DataValue* input, bool isMainDirection)
    {
        return isMainDirection ? getDataValue<T, U>(input) : getReverseDataValue<T, U>(input);
    };
};
} // namespace rive

#endif