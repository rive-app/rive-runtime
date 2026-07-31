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
        m_TrackValue = value;
        trackValueChanged();
        notifyPropertyChanged(trackValuePropertyKey);
    }

    inline float trackMaxValue() const { return m_TrackMaxValue; }
    void trackMaxValue(float value)
    {
        if (m_TrackMaxValue == value)
        {
            return;
        }
        m_TrackMaxValue = value;
        trackMaxValueChanged();
        notifyPropertyChanged(trackMaxValuePropertyKey);
    }

    inline uint8_t collection() const { return m_Collection; }
    void collection(uint8_t value)
    {
        if (m_Collection == value)
        {
            return;
        }
        m_Collection = value;
        collectionChanged();
        notifyPropertyChanged(collectionPropertyKey);
    }

    inline uint8_t trackType() const { return m_TrackType; }
    void trackType(uint8_t value)
    {
        if (m_TrackType == value)
        {
            return;
        }
        m_TrackType = value;
        trackTypeChanged();
        notifyPropertyChanged(trackTypePropertyKey);
    }

    inline uint8_t trackMaxType() const { return m_TrackMaxType; }
    void trackMaxType(uint8_t value)
    {
        if (m_TrackMaxType == value)
        {
            return;
        }
        m_TrackMaxType = value;
        trackMaxTypeChanged();
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
};
} // namespace rive

#endif