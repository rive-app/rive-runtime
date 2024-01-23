#ifndef _RIVE_AUDIO_EVENT_BASE_HPP_
#define _RIVE_AUDIO_EVENT_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/event.hpp"
namespace rive
{
class AudioEventBase : public Event
{
protected:
    typedef Event Super;

public:
    static const uint16_t typeKey = 407;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AudioEventBase::typeKey:
            case EventBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t assetIdPropertyKey = 408;

private:
    uint32_t m_AssetId = -1;

public:
    inline uint32_t assetId() const { return m_AssetId; }
    void assetId(uint32_t value)
    {
        if (m_AssetId == value)
        {
            return;
        }
        m_AssetId = value;
        assetIdChanged();
    }

    Core* clone() const override;
    void copy(const AudioEventBase& object)
    {
        m_AssetId = object.m_AssetId;
        Event::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreUintType::deserialize(reader);
                return true;
        }
        return Event::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
};
} // namespace rive

#endif