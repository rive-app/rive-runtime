#ifndef _RIVE_TARGET_EFFECT_BASE_HPP_
#define _RIVE_TARGET_EFFECT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
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
    Id m_TargetId = kEmptyId;

public:
    inline Id targetId() const { return m_TargetId; }
    void targetId(Id value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(targetIdPropertyKey, &m_TargetId, &value);
        m_TargetId = value;
        RIVE_EDITOR_CHANGED(targetIdChanged());
        notifyPropertyChanged(targetIdPropertyKey);
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
                m_TargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/paint/target_effect_ext.inl"
#endif
};
} // namespace rive

#endif