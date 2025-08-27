#ifndef _RIVE_DATA_CONVERTER_SYSTEM_NORMALIZER_HPP_
#define _RIVE_DATA_CONVERTER_SYSTEM_NORMALIZER_HPP_
#include "rive/generated/data_bind/converters/data_converter_system_normalizer_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterSystemNormalizer : public DataConverterSystemNormalizerBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
};
} // namespace rive

#endif