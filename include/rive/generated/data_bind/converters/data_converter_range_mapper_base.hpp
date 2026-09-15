#ifndef _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#define _RIVE_DATA_CONVERTER_RANGE_MAPPER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
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
    Id m_InterpolatorId = kEmptyId;
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
        RIVE_EDITOR_CHANGING(interpolationTypePropertyKey,
                             &m_InterpolationType,
                             &value);
        m_InterpolationType = value;
        RIVE_EDITOR_CHANGED(interpolationTypeChanged());
        notifyPropertyChanged(interpolationTypePropertyKey);
    }

    inline Id interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(Id value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolatorIdPropertyKey,
                             &m_InterpolatorId,
                             &value);
        m_InterpolatorId = value;
        RIVE_EDITOR_CHANGED(interpolatorIdChanged());
        notifyPropertyChanged(interpolatorIdPropertyKey);
    }

    inline uint32_t flags() const { return m_Flags; }
    void flags(uint32_t value)
    {
        if (m_Flags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flagsPropertyKey, &m_Flags, &value);
        m_Flags = value;
        RIVE_EDITOR_CHANGED(flagsChanged());
        notifyPropertyChanged(flagsPropertyKey);
    }

    inline float minInput() const { return m_MinInput; }
    void minInput(float value)
    {
        if (m_MinInput == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minInputPropertyKey, &m_MinInput, &value);
        m_MinInput = value;
        RIVE_EDITOR_CHANGED(minInputChanged());
        notifyPropertyChanged(minInputPropertyKey);
    }

    inline float maxInput() const { return m_MaxInput; }
    void maxInput(float value)
    {
        if (m_MaxInput == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxInputPropertyKey, &m_MaxInput, &value);
        m_MaxInput = value;
        RIVE_EDITOR_CHANGED(maxInputChanged());
        notifyPropertyChanged(maxInputPropertyKey);
    }

    inline float minOutput() const { return m_MinOutput; }
    void minOutput(float value)
    {
        if (m_MinOutput == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minOutputPropertyKey, &m_MinOutput, &value);
        m_MinOutput = value;
        RIVE_EDITOR_CHANGED(minOutputChanged());
        notifyPropertyChanged(minOutputPropertyKey);
    }

    inline float maxOutput() const { return m_MaxOutput; }
    void maxOutput(float value)
    {
        if (m_MaxOutput == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxOutputPropertyKey, &m_MaxOutput, &value);
        m_MaxOutput = value;
        RIVE_EDITOR_CHANGED(maxOutputChanged());
        notifyPropertyChanged(maxOutputPropertyKey);
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
                m_InterpolatorId = CoreIdType::runtimeDeserialize(reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_range_mapper_ext.inl"
#endif
};
} // namespace rive

#endif