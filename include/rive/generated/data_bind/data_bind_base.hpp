#ifndef _RIVE_DATA_BIND_BASE_HPP_
#define _RIVE_DATA_BIND_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class DataBindBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 446;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataBindBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 585;
    static const uint16_t propertyKeyPropertyKey = 586;
    static const uint16_t modeValuePropertyKey = 587;

private:
    uint32_t m_TargetId = -1;
    uint32_t m_PropertyKey = Core::invalidPropertyKey;
    uint32_t m_ModeValue = 0;

public:
    inline uint32_t targetId() const { return m_TargetId; }
    void targetId(uint32_t value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        m_TargetId = value;
        targetIdChanged();
    }

    inline uint32_t propertyKey() const { return m_PropertyKey; }
    void propertyKey(uint32_t value)
    {
        if (m_PropertyKey == value)
        {
            return;
        }
        m_PropertyKey = value;
        propertyKeyChanged();
    }

    inline uint32_t modeValue() const { return m_ModeValue; }
    void modeValue(uint32_t value)
    {
        if (m_ModeValue == value)
        {
            return;
        }
        m_ModeValue = value;
        modeValueChanged();
    }

    Core* clone() const override;
    void copy(const DataBindBase& object)
    {
        m_TargetId = object.m_TargetId;
        m_PropertyKey = object.m_PropertyKey;
        m_ModeValue = object.m_ModeValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
            case propertyKeyPropertyKey:
                m_PropertyKey = CoreUintType::deserialize(reader);
                return true;
            case modeValuePropertyKey:
                m_ModeValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
    virtual void propertyKeyChanged() {}
    virtual void modeValueChanged() {}
};
} // namespace rive

#endif