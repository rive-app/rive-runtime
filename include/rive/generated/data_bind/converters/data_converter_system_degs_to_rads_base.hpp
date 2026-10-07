#ifndef _RIVE_DATA_CONVERTER_SYSTEM_DEGS_TO_RADS_BASE_HPP_
#define _RIVE_DATA_CONVERTER_SYSTEM_DEGS_TO_RADS_BASE_HPP_
#include "rive/data_bind/converters/data_converter_operation_value.hpp"
namespace rive
{
class DataConverterSystemDegsToRadsBase : public DataConverterOperationValue
{
protected:
    typedef DataConverterOperationValue Super;

public:
    static const uint16_t typeKey = 514;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif