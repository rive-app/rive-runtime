#ifndef _RIVE_DATA_CONVERTER_STRING_TRIM_BASE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_TRIM_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterStringTrimBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 532;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterStringTrimBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t trimTypePropertyKey = 746;

protected:
    uint32_t m_TrimType = 1;

public:
    inline uint32_t trimType() const { return m_TrimType; }
    void trimType(uint32_t value)
    {
        if (m_TrimType == value)
        {
            return;
        }
        m_TrimType = value;
        trimTypeChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterStringTrimBase& object)
    {
        m_TrimType = object.m_TrimType;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case trimTypePropertyKey:
                m_TrimType = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void trimTypeChanged() {}
};
} // namespace rive

#endif