#ifndef _RIVE_TEXT_TARGET_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_TARGET_MODIFIER_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/text/text_modifier.hpp"
namespace rive
{
class TextTargetModifierBase : public TextModifier
{
protected:
    typedef TextModifier Super;

public:
    static const uint16_t typeKey = 546;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextTargetModifierBase::typeKey:
            case TextModifierBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 778;

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

    void copy(const TextTargetModifierBase& object)
    {
        m_TargetId = object.m_TargetId;
        TextModifier::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
        }
        return TextModifier::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
};
} // namespace rive

#endif