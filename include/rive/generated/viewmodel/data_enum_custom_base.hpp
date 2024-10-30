#ifndef _RIVE_DATA_ENUM_CUSTOM_BASE_HPP_
#define _RIVE_DATA_ENUM_CUSTOM_BASE_HPP_
#include "rive/viewmodel/data_enum.hpp"
namespace rive
{
class DataEnumCustomBase : public DataEnum
{
protected:
    typedef DataEnum Super;

public:
    static const uint16_t typeKey = 438;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataEnumCustomBase::typeKey:
            case DataEnumBase::typeKey:
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