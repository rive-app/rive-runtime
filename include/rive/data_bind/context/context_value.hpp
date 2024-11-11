#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContextValue
{
protected:
    DataBind* m_dataBind;
    DataValue* m_dataValue;

public:
    DataBindContextValue(DataBind* dataBind);
    virtual ~DataBindContextValue(){};
    virtual void applyToSource(Core* component,
                               uint32_t propertyKey,
                               bool isMainDirection);
    virtual void apply(Core* component,
                       uint32_t propertyKey,
                       bool isMainDirection){};
    virtual void update(Core* component){};
    virtual void dispose(){};
    virtual DataValue* getTargetValue(Core* target, uint32_t propertyKey)
    {
        return nullptr;
    };
    void updateSourceValue();
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
        return isMainDirection ? getDataValue<T, U>(input, dataBind)
                               : getReverseDataValue<T, U>(input, dataBind);
    };
};
} // namespace rive

#endif