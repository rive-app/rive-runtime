#ifndef _RIVE_DATA_CONVERTER_ROUNDER_BASE_HPP_
#define _RIVE_DATA_CONVERTER_ROUNDER_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterRounderBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 489;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterRounderBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t decimalsPropertyKey = 669;

private:
    uint32_t m_Decimals = 0;

public:
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

    Core* clone() const override;
    void copy(const DataConverterRounderBase& object)
    {
        m_Decimals = object.m_Decimals;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case decimalsPropertyKey:
                m_Decimals = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void decimalsChanged() {}
};
} // namespace rive

#endif