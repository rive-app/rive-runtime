#ifndef _RIVE_OPEN_URL_EVENT_BASE_HPP_
#define _RIVE_OPEN_URL_EVENT_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/event.hpp"
namespace rive
{
class OpenUrlEventBase : public Event
{
protected:
    typedef Event Super;

public:
    static const uint16_t typeKey = 131;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case OpenUrlEventBase::typeKey:
            case EventBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t urlPropertyKey = 248;
    static const uint16_t targetValuePropertyKey = 249;

private:
    std::string m_Url = "";
    uint32_t m_TargetValue = 0;

public:
    inline const std::string& url() const { return m_Url; }
    void url(std::string value)
    {
        if (m_Url == value)
        {
            return;
        }
        m_Url = value;
        urlChanged();
    }

    inline uint32_t targetValue() const { return m_TargetValue; }
    void targetValue(uint32_t value)
    {
        if (m_TargetValue == value)
        {
            return;
        }
        m_TargetValue = value;
        targetValueChanged();
    }

    Core* clone() const override;
    void copy(const OpenUrlEventBase& object)
    {
        m_Url = object.m_Url;
        m_TargetValue = object.m_TargetValue;
        Event::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case urlPropertyKey:
                m_Url = CoreStringType::deserialize(reader);
                return true;
            case targetValuePropertyKey:
                m_TargetValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Event::deserialize(propertyKey, reader);
    }

protected:
    virtual void urlChanged() {}
    virtual void targetValueChanged() {}
};
} // namespace rive

#endif