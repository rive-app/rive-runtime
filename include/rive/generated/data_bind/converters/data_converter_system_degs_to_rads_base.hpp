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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterSystemDegsToRadsBase::typeKey:
            case DataConverterOperationValueBase::typeKey:
            case DataConverterOperationBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif