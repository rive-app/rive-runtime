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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterFormulaBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

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
        m_RandomModeValue = value;
        randomModeValueChanged();
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
    virtual void randomModeValueChanged() {}
};
} // namespace rive

#endif