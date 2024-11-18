#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_VALUES_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_VALUES_HPP_
#include "rive/generated/data_bind/converters/data_converter_range_mapper_values_base.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterRangeMapperValues : public DataConverterRangeMapperValuesBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
};
} // namespace rive

#endif