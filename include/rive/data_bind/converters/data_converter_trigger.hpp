#ifndef _RIVE_DATA_CONVERTER_TRIGGER_HPP_
#define _RIVE_DATA_CONVERTER_TRIGGER_HPP_
#include "rive/generated/data_bind/converters/data_converter_trigger_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterTrigger : public DataConverterTriggerBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::trigger; };
    DataValueTrigger m_output;
};
} // namespace rive

#endif