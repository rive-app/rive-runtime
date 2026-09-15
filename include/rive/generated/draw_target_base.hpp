#ifndef _RIVE_DRAW_TARGET_BASE_HPP_
#define _RIVE_DRAW_TARGET_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class DrawTargetBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 48;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DrawTargetBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t drawableIdPropertyKey = 119;
    static const uint16_t placementValuePropertyKey = 120;

protected:
    Id m_DrawableId = kEmptyId;
    uint32_t m_PlacementValue = 0;

public:
    inline Id drawableId() const { return m_DrawableId; }
    void drawableId(Id value)
    {
        if (m_DrawableId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(drawableIdPropertyKey, &m_DrawableId, &value);
        m_DrawableId = value;
        RIVE_EDITOR_CHANGED(drawableIdChanged());
        notifyPropertyChanged(drawableIdPropertyKey);
    }

    inline uint32_t placementValue() const { return m_PlacementValue; }
    void placementValue(uint32_t value)
    {
        if (m_PlacementValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(placementValuePropertyKey,
                             &m_PlacementValue,
                             &value);
        m_PlacementValue = value;
        RIVE_EDITOR_CHANGED(placementValueChanged());
        notifyPropertyChanged(placementValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const DrawTargetBase& object)
    {
        m_DrawableId = object.m_DrawableId;
        m_PlacementValue = object.m_PlacementValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case drawableIdPropertyKey:
                m_DrawableId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case placementValuePropertyKey:
                m_PlacementValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void drawableIdChanged() {}
    virtual void placementValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/draw_target_ext.inl"
#endif
};
} // namespace rive

#endif