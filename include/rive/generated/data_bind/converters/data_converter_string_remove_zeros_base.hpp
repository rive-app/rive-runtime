#ifndef _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_BASE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_BASE_HPP_
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterStringRemoveZerosBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 531;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterStringRemoveZerosBase::typeKey:
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