#ifndef _RIVE_DRAW_RULES_BASE_HPP_
#define _RIVE_DRAW_RULES_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class DrawRulesBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 49;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DrawRulesBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t drawTargetIdPropertyKey = 121;

protected:
    Id m_DrawTargetId = kEmptyId;

public:
    inline Id drawTargetId() const { return m_DrawTargetId; }
    void drawTargetId(Id value)
    {
        if (m_DrawTargetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(drawTargetIdPropertyKey, &m_DrawTargetId, &value);
        m_DrawTargetId = value;
        RIVE_EDITOR_CHANGED(drawTargetIdChanged());
        notifyPropertyChanged(drawTargetIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const DrawRulesBase& object)
    {
        m_DrawTargetId = object.m_DrawTargetId;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case drawTargetIdPropertyKey:
                m_DrawTargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void drawTargetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/draw_rules_ext.inl"
#endif
};
} // namespace rive

#endif