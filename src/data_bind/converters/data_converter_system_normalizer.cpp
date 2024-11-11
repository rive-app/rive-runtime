#include "rive/data_bind/converters/data_converter_system_normalizer.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind_flags.hpp"

using namespace rive;

DataValue* DataConverterSystemNormalizer::convert(DataValue* input,
                                                  DataBind* dataBind)
{
    auto flagsValue = static_cast<DataBindFlags>(dataBind->flags());
    if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToSource))
    {
        return DataConverterOperation::reverseConvert(input, dataBind);
    }
    return DataConverterOperation::convert(input, dataBind);
}

DataValue* DataConverterSystemNormalizer::reverseConvert(DataValue* input,
                                                         DataBind* dataBind)
{
    auto flagsValue = static_cast<DataBindFlags>(dataBind->flags());
    if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToTarget))
    {
        return DataConverterOperation::convert(input, dataBind);
    }
    return DataConverterOperation::reverseConvert(input, dataBind);
}