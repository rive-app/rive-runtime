#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_VALUES_BASE_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_VALUES_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
namespace rive
{
class DataConverterRangeMapperValuesBase : public DataConverterRangeMapper
{
protected:
    typedef DataConverterRangeMapper Super;

public:
    static const uint16_t typeKey = 519;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterRangeMapperValuesBase::typeKey:
            case DataConverterRangeMapperBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t minInputPropertyKey = 716;
    static const uint16_t maxInputPropertyKey = 717;
    static const uint16_t minOutputPropertyKey = 718;
    static const uint16_t maxOutputPropertyKey = 719;

protected:
    float m_MinInput = 1.0f;
    float m_MaxInput = 1.0f;
    float m_MinOutput = 1.0f;
    float m_MaxOutput = 1.0f;

public:
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
    void copy(const DataConverterRangeMapperValuesBase& object)
    {
        m_MinInput = object.m_MinInput;
        m_MaxInput = object.m_MaxInput;
        m_MinOutput = object.m_MinOutput;
        m_MaxOutput = object.m_MaxOutput;
        DataConverterRangeMapper::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
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
        return DataConverterRangeMapper::deserialize(propertyKey, reader);
    }

protected:
    virtual void minInputChanged() {}
    virtual void maxInputChanged() {}
    virtual void minOutputChanged() {}
    virtual void maxOutputChanged() {}
};
} // namespace rive

#endif