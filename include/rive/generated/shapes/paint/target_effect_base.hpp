#ifndef _RIVE_TARGET_EFFECT_BASE_HPP_
#define _RIVE_TARGET_EFFECT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TargetEffectBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 644;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TargetEffectBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 922;

protected:
    uint32_t m_TargetId = -1;

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

    Core* clone() const override;
    void copy(const TargetEffectBase& object)
    {
        m_TargetId = object.m_TargetId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
};
} // namespace rive

#endif