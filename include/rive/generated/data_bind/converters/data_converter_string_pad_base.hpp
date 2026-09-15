#ifndef _RIVE_DATA_CONVERTER_STRING_PAD_BASE_HPP_
#define _RIVE_DATA_CONVERTER_STRING_PAD_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterStringPadBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 530;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterStringPadBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t lengthPropertyKey = 743;
    static const uint16_t textPropertyKey = 744;
    static const uint16_t padTypePropertyKey = 745;

protected:
    uint32_t m_Length = 1;
    std::string m_Text = "";
    uint32_t m_PadType = 0;

public:
    inline uint32_t length() const { return m_Length; }
    void length(uint32_t value)
    {
        if (m_Length == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(lengthPropertyKey, &m_Length, &value);
        m_Length = value;
        RIVE_EDITOR_CHANGED(lengthChanged());
        notifyPropertyChanged(lengthPropertyKey);
    }

    inline const std::string& text() const { return m_Text; }
    void text(std::string value)
    {
        if (m_Text == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(textPropertyKey, m_Text, value);
        m_Text = value;
        RIVE_EDITOR_CHANGED(textChanged());
        notifyPropertyChanged(textPropertyKey);
    }

    inline uint32_t padType() const { return m_PadType; }
    void padType(uint32_t value)
    {
        if (m_PadType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(padTypePropertyKey, &m_PadType, &value);
        m_PadType = value;
        RIVE_EDITOR_CHANGED(padTypeChanged());
        notifyPropertyChanged(padTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const DataConverterStringPadBase& object)
    {
        m_Length = object.m_Length;
        m_Text = object.m_Text;
        m_PadType = object.m_PadType;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case lengthPropertyKey:
                m_Length = CoreUintType::deserialize(reader);
                return true;
            case textPropertyKey:
                m_Text = CoreStringType::deserialize(reader);
                return true;
            case padTypePropertyKey:
                m_PadType = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void lengthChanged() {}
    virtual void textChanged() {}
    virtual void padTypeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_string_pad_ext.inl"
#endif
};
} // namespace rive

#endif