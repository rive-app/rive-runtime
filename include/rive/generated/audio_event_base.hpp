#ifndef _RIVE_AUDIO_EVENT_BASE_HPP_
#define _RIVE_AUDIO_EVENT_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/event.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class AudioEventBase : public Event
{
protected:
    typedef Event Super;

public:
    static const uint16_t typeKey = 407;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AudioEventBase::typeKey:
            case EventBase::typeKey:
            case CustomPropertyGroupBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t assetIdPropertyKey = 408;

protected:
    Id m_AssetId = kEmptyId;

public:
    inline Id assetId() const { return m_AssetId; }
    void assetId(Id value)
    {
        if (m_AssetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(assetIdPropertyKey, &m_AssetId, &value);
        m_AssetId = value;
        RIVE_EDITOR_CHANGED(assetIdChanged());
        notifyPropertyChanged(assetIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const AudioEventBase& object)
    {
        m_AssetId = object.m_AssetId;
        RIVE_EDITOR_COPY(object);
        Event::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Event::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/audio_event_ext.inl"
#endif
};
} // namespace rive

#endif