#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterRangeMapperBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 519;

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
    static const uint16_t minInputPropertyKey = 716;
    static const uint16_t maxInputPropertyKey = 717;
    static const uint16_t minOutputPropertyKey = 718;
    static const uint16_t maxOutputPropertyKey = 719;

protected:
    uint32_t m_InterpolationType = 1;
    uint32_t m_InterpolatorId = -1;
    uint32_t m_Flags = 0;
    float m_MinInput = 1.0f;
    float m_MaxInput = 1.0f;
    float m_MinOutput = 1.0f;
    float m_MaxOutput = 1.0f;

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

    inline float minInput() const { return m_MinInput; }
    void minInput(float value)
    {
        if (m_MinInput == value)
        {
            return;
        }
        m_MinInput = value;
        minInputChanged();
    }

    inline float maxInput() const { return m_MaxInput; }
    void maxInput(float value)
    {
        if (m_MaxInput == value)
        {
            return;
        }
        m_MaxInput = value;
        maxInputChanged();
    }

    inline float minOutput() const { return m_MinOutput; }
    void minOutput(float value)
    {
        if (m_MinOutput == value)
        {
            return;
        }
        m_MinOutput = value;
        minOutputChanged();
    }

    inline float maxOutput() const { return m_MaxOutput; }
    void maxOutput(float value)
    {
        if (m_MaxOutput == value)
        {
            return;
        }
        m_MaxOutput = value;
        maxOutputChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterRangeMapperBase& object)
    {
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        m_Flags = object.m_Flags;
        m_MinInput = object.m_MinInput;
        m_MaxInput = object.m_MaxInput;
        m_MinOutput = object.m_MinOutput;
        m_MaxOutput = object.m_MaxOutput;
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
            case minInputPropertyKey:
                m_MinInput = CoreDoubleType::deserialize(reader);
                return true;
            case maxInputPropertyKey:
                m_MaxInput = CoreDoubleType::deserialize(reader);
                return true;
            case minOutputPropertyKey:
                m_MinOutput = CoreDoubleType::deserialize(reader);
                return true;
            case maxOutputPropertyKey:
                m_MaxOutput = CoreDoubleType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void flagsChanged() {}
    virtual void minInputChanged() {}
    virtual void maxInputChanged() {}
    virtual void minOutputChanged() {}
    virtual void maxOutputChanged() {}
};
} // namespace rive

#endif