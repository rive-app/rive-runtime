#ifndef _RIVE_DATA_CONVERTER_SYSTEM_DEGS_TO_RADS_HPP_
#define _RIVE_DATA_CONVERTER_SYSTEM_DEGS_TO_RADS_HPP_
#include "rive/generated/data_bind/converters/data_converter_system_degs_to_rads_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterSystemDegsToRads : public DataConverterSystemDegsToRadsBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
};
} // namespace rive

#endif