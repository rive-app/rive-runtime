#ifndef _RIVE_GRID_TRACK_BASE_HPP_
#define _RIVE_GRID_TRACK_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class GridTrackBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 1058;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case GridTrackBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t trackValuePropertyKey = 1063;
    static const uint16_t trackMaxValuePropertyKey = 1065;
    static const uint16_t collectionPropertyKey = 1061;
    static const uint16_t trackTypePropertyKey = 1062;
    static const uint16_t trackMaxTypePropertyKey = 1064;

protected:
    float m_TrackValue = 0.0f;
    float m_TrackMaxValue = 0.0f;
    uint8_t m_Collection = 0;
    uint8_t m_TrackType = 0;
    uint8_t m_TrackMaxType = 0;

public:
    inline float trackValue() const { return m_TrackValue; }
    void trackValue(float value)
    {
        if (m_TrackValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(trackValuePropertyKey, &m_TrackValue, &value);
        m_TrackValue = value;
        RIVE_EDITOR_CHANGED(trackValueChanged());
        notifyPropertyChanged(trackValuePropertyKey);
    }

    inline float trackMaxValue() const { return m_TrackMaxValue; }
    void trackMaxValue(float value)
    {
        if (m_TrackMaxValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(trackMaxValuePropertyKey,
                             &m_TrackMaxValue,
                             &value);
        m_TrackMaxValue = value;
        RIVE_EDITOR_CHANGED(trackMaxValueChanged());
        notifyPropertyChanged(trackMaxValuePropertyKey);
    }

    inline uint8_t collection() const { return m_Collection; }
    void collection(uint8_t value)
    {
        if (m_Collection == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(collectionPropertyKey, &m_Collection, &value);
        m_Collection = value;
        RIVE_EDITOR_CHANGED(collectionChanged());
        notifyPropertyChanged(collectionPropertyKey);
    }

    inline uint8_t trackType() const { return m_TrackType; }
    void trackType(uint8_t value)
    {
        if (m_TrackType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(trackTypePropertyKey, &m_TrackType, &value);
        m_TrackType = value;
        RIVE_EDITOR_CHANGED(trackTypeChanged());
        notifyPropertyChanged(trackTypePropertyKey);
    }

    inline uint8_t trackMaxType() const { return m_TrackMaxType; }
    void trackMaxType(uint8_t value)
    {
        if (m_TrackMaxType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(trackMaxTypePropertyKey, &m_TrackMaxType, &value);
        m_TrackMaxType = value;
        RIVE_EDITOR_CHANGED(trackMaxTypeChanged());
        notifyPropertyChanged(trackMaxTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const GridTrackBase& object)
    {
        m_TrackValue = object.m_TrackValue;
        m_TrackMaxValue = object.m_TrackMaxValue;
        m_Collection = object.m_Collection;
        m_TrackType = object.m_TrackType;
        m_TrackMaxType = object.m_TrackMaxType;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case trackValuePropertyKey:
                m_TrackValue = CoreDoubleType::deserialize(reader);
                return true;
            case trackMaxValuePropertyKey:
                m_TrackMaxValue = CoreDoubleType::deserialize(reader);
                return true;
            case collectionPropertyKey:
                m_Collection = CoreUintType::deserialize(reader);
                return true;
            case trackTypePropertyKey:
                m_TrackType = CoreUintType::deserialize(reader);
                return true;
            case trackMaxTypePropertyKey:
                m_TrackMaxType = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void trackValueChanged() {}
    virtual void trackMaxValueChanged() {}
    virtual void collectionChanged() {}
    virtual void trackTypeChanged() {}
    virtual void trackMaxTypeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/grid_track_ext.inl"
#endif
};
} // namespace rive

#endif