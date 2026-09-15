#ifndef _RIVE_FOCUS_DATA_BASE_HPP_
#define _RIVE_FOCUS_DATA_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class FocusDataBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 653;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FocusDataBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t focusFlagsPropertyKey = 1033;
    static const uint16_t canFocusPropertyKey = 953;
    static const uint32_t canFocusBitmask = 1u << 0;
    static const uint16_t canTouchPropertyKey = 954;
    static const uint32_t canTouchBitmask = 1u << 1;
    static const uint16_t canTraversePropertyKey = 955;
    static const uint32_t canTraverseBitmask = 1u << 2;
    static const uint16_t edgeBehaviorValuePropertyKey = 956;

protected:
    uint32_t m_FocusFlags = 7;
    uint32_t m_EdgeBehaviorValue = 0;

public:
    inline uint32_t focusFlags() const { return m_FocusFlags; }
    void focusFlags(uint32_t value)
    {
        if (m_FocusFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(focusFlagsPropertyKey, &m_FocusFlags, &value);
        m_FocusFlags = value;
        RIVE_EDITOR_CHANGED(focusFlagsChanged());
        notifyPropertyChanged(focusFlagsPropertyKey);
    }

    inline bool canFocus() const
    {
        return (m_FocusFlags & canFocusBitmask) != 0;
    }
    void canFocus(bool value)
    {
        const bool prev = (m_FocusFlags & canFocusBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(canFocusPropertyKey, &prev, &value);
        m_FocusFlags = value ? (m_FocusFlags | canFocusBitmask)
                             : (m_FocusFlags & ~canFocusBitmask);
        RIVE_EDITOR_CHANGED(focusFlagsChanged());
        notifyPropertyChanged(focusFlagsPropertyKey);
    }
    inline bool canTouch() const
    {
        return (m_FocusFlags & canTouchBitmask) != 0;
    }
    void canTouch(bool value)
    {
        const bool prev = (m_FocusFlags & canTouchBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(canTouchPropertyKey, &prev, &value);
        m_FocusFlags = value ? (m_FocusFlags | canTouchBitmask)
                             : (m_FocusFlags & ~canTouchBitmask);
        RIVE_EDITOR_CHANGED(focusFlagsChanged());
        notifyPropertyChanged(focusFlagsPropertyKey);
    }
    inline bool canTraverse() const
    {
        return (m_FocusFlags & canTraverseBitmask) != 0;
    }
    void canTraverse(bool value)
    {
        const bool prev = (m_FocusFlags & canTraverseBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(canTraversePropertyKey, &prev, &value);
        m_FocusFlags = value ? (m_FocusFlags | canTraverseBitmask)
                             : (m_FocusFlags & ~canTraverseBitmask);
        RIVE_EDITOR_CHANGED(focusFlagsChanged());
        notifyPropertyChanged(focusFlagsPropertyKey);
    }
    inline uint32_t edgeBehaviorValue() const { return m_EdgeBehaviorValue; }
    void edgeBehaviorValue(uint32_t value)
    {
        if (m_EdgeBehaviorValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(edgeBehaviorValuePropertyKey,
                             &m_EdgeBehaviorValue,
                             &value);
        m_EdgeBehaviorValue = value;
        RIVE_EDITOR_CHANGED(edgeBehaviorValueChanged());
        notifyPropertyChanged(edgeBehaviorValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const FocusDataBase& object)
    {
        m_FocusFlags = object.m_FocusFlags;
        m_EdgeBehaviorValue = object.m_EdgeBehaviorValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case focusFlagsPropertyKey:
                m_FocusFlags = CoreUintType::deserialize(reader);
                return true;
            case edgeBehaviorValuePropertyKey:
                m_EdgeBehaviorValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void focusFlagsChanged() {}
    virtual void edgeBehaviorValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/focus_data_ext.inl"
#endif
};
} // namespace rive

#endif