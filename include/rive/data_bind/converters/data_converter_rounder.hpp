#ifndef _RIVE_DATA_CONVERTER_ROUND_HPP_
#define _RIVE_DATA_CONVERTER_ROUND_HPP_
#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterRounder : public DataConverterRounderBase
{
public:
    DataValue* convert(DataValue* value) override;
    DataType outputType() override { return DataType::number; };
};
} // namespace rive

#endif