#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterRangeMapperBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 518;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterRangeMapperBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t interpolationTypePropertyKey = 713;
    static const uint16_t interpolatorIdPropertyKey = 714;
    static const uint16_t flagsPropertyKey = 715;

protected:
    uint32_t m_InterpolationType = 0;
    uint32_t m_InterpolatorId = -1;
    uint32_t m_Flags = 0;

public:
    inline uint32_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint32_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        m_InterpolationType = value;
        interpolationTypeChanged();
    }

    inline uint32_t interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(uint32_t value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        m_InterpolatorId = value;
        interpolatorIdChanged();
    }

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

    void copy(const DataConverterRangeMapperBase& object)
    {
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        m_Flags = object.m_Flags;
        DataConverter::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case interpolationTypePropertyKey:
                m_InterpolationType = CoreUintType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreUintType::deserialize(reader);
                return true;
            case flagsPropertyKey:
                m_Flags = CoreUintType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void flagsChanged() {}
};
} // namespace rive

#endif