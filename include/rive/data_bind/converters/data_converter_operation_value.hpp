#ifndef _RIVE_DATA_CONVERTER_OPERATION_VALUE_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_VALUE_HPP_
#include "rive/generated/data_bind/converters/data_converter_operation_value_base.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterOperationValue : public DataConverterOperationValueBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    void operationValueChanged() override;
};
} // namespace rive

#endif