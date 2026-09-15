#ifndef _RIVE_DATA_CONVERTER_INTERPOLATOR_BASE_HPP_
#define _RIVE_DATA_CONVERTER_INTERPOLATOR_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
namespace rive
{
class DataConverterInterpolatorBase : public DataConverter
{
protected:
    typedef DataConverter Super;

public:
    static const uint16_t typeKey = 534;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterInterpolatorBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t interpolationTypePropertyKey = 757;
    static const uint16_t interpolatorIdPropertyKey = 758;
    static const uint16_t durationPropertyKey = 756;

protected:
    uint32_t m_InterpolationType = 1;
    Id m_InterpolatorId = kEmptyId;
    float m_Duration = 1.0f;

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

    inline float duration() const { return m_Duration; }
    void duration(float value)
    {
        if (m_Duration == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(durationPropertyKey, &m_Duration, &value);
        m_Duration = value;
        RIVE_EDITOR_CHANGED(durationChanged());
        notifyPropertyChanged(durationPropertyKey);
    }

    Core* clone() const override;
    void copy(const DataConverterInterpolatorBase& object)
    {
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        m_Duration = object.m_Duration;
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
            case durationPropertyKey:
                m_Duration = CoreDoubleType::deserialize(reader);
                return true;
        }
        return DataConverter::deserialize(propertyKey, reader);
    }

protected:
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void durationChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/data_converter_interpolator_ext.inl"
#endif
};
} // namespace rive

#endif