#ifndef _RIVE_DATA_CONVERTER_ROUND_HPP_
#define _RIVE_DATA_CONVERTER_ROUND_HPP_
#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterRounder : public DataConverterRounderBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::number; };

private:
    DataValueNumber m_output;
};
} // namespace rive

#endif