#include "rive/data_bind/converters/data_converter_system_degs_to_rads.hpp"
#include "rive/math/math_types.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind_flags.hpp"

using namespace rive;

DataValue* DataConverterSystemDegsToRads::convert(DataValue* input,
                                                  DataBind* dataBind)
{
    auto flagsValue = static_cast<DataBindFlags>(dataBind->flags());
    if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToSource))
    {
        return DataConverterOperation::reverseConvert(input, dataBind);
    }
    return DataConverterOperation::convert(input, dataBind);
}

DataValue* DataConverterSystemDegsToRads::reverseConvert(DataValue* input,
                                                         DataBind* dataBind)
{
    auto flagsValue = static_cast<DataBindFlags>(dataBind->flags());
    if (((flagsValue & DataBindFlags::Direction) == DataBindFlags::ToTarget))
    {
        return DataConverterOperation::convert(input, dataBind);
    }
    return DataConverterOperation::reverseConvert(input, dataBind);
}