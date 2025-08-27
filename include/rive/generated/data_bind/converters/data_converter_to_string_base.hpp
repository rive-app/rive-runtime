#ifndef _RIVE_DATA_CONVERTER_TO_STRING_BASE_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterToStringBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 490;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterToStringBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t flagsPropertyKey = 764;
    static const uint16_t decimalsPropertyKey = 765;
    static const uint16_t colorFormatPropertyKey = 766;

protected:
    uint32_t m_Flags = 0;
    uint32_t m_Decimals = 0;
    std::string m_ColorFormat = "";

public:
    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        m_Flags = value;
        flagsChanged();
    }

    inline uint32_t decimals() const { return m_Decimals; }
    void decimals(uint32_t value)
    {
        if (m_Decimals == value)
        {
            return;
        }
        m_Decimals = value;
        decimalsChanged();
    }

    inline const std::string& colorFormat() const { return m_ColorFormat; }
    void colorFormat(std::string value)
    {
        if (m_ColorFormat == value)
        {
            return;
        }
        m_ColorFormat = value;
        colorFormatChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterToStringBase& object)
    {
        m_Flags = object.m_Flags;
        m_Decimals = object.m_Decimals;
        m_ColorFormat = object.m_ColorFormat;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
            case decimalsPropertyKey:
                m_Decimals = CoreUintType::deserialize(reader);
                return true;
            case colorFormatPropertyKey:
                m_ColorFormat = CoreStringType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void flagsChanged() {}
    virtual void decimalsChanged() {}
    virtual void colorFormatChanged() {}
};
} // namespace rive

#endif