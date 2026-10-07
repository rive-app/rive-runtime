#ifndef _RIVE_DATA_CONVERTER_FORMULA_BASE_HPP_
#define _RIVE_DATA_CONVERTER_FORMULA_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterFormulaBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 536;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t randomModeValuePropertyKey = 887;

protected:
    uint32_t m_RandomModeValue = 0;

public:
    inline uint32_t randomModeValue() const { return m_RandomModeValue; }
    void randomModeValue(uint32_t value)
    {
        if (m_RandomModeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(randomModeValuePropertyKey,
                             &m_RandomModeValue,
                             &value);
        m_RandomModeValue = value;
        notifyPropertyChanged(randomModeValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const DataConverterFormulaBase& object)
    {
        m_RandomModeValue = object.m_RandomModeValue;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case randomModeValuePropertyKey:
                m_RandomModeValue = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_formula_ext.inl"
#endif
};
} // namespace rive

#endif