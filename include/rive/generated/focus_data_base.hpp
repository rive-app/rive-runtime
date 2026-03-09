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

    static const uint16_t canFocusPropertyKey = 953;
    static const uint16_t canTouchPropertyKey = 954;
    static const uint16_t canTraversePropertyKey = 955;
    static const uint16_t edgeBehaviorValuePropertyKey = 956;

protected:
    bool m_CanFocus = true;
    bool m_CanTouch = true;
    bool m_CanTraverse = true;
    uint32_t m_EdgeBehaviorValue = 0;

public:
    inline bool canFocus() const { return m_CanFocus; }
    void canFocus(bool value)
    {
        if (m_CanFocus == value)
        {
            return;
        }
        m_CanFocus = value;
        canFocusChanged();
    }

    inline bool canTouch() const { return m_CanTouch; }
    void canTouch(bool value)
    {
        if (m_CanTouch == value)
        {
            return;
        }
        m_CanTouch = value;
        canTouchChanged();
    }

    inline bool canTraverse() const { return m_CanTraverse; }
    void canTraverse(bool value)
    {
        if (m_CanTraverse == value)
        {
            return;
        }
        m_CanTraverse = value;
        canTraverseChanged();
    }

    inline uint32_t edgeBehaviorValue() const { return m_EdgeBehaviorValue; }
    void edgeBehaviorValue(uint32_t value)
    {
        if (m_EdgeBehaviorValue == value)
        {
            return;
        }
        m_EdgeBehaviorValue = value;
        edgeBehaviorValueChanged();
    }

    Core* clone() const override;
    void copy(const FocusDataBase& object)
    {
        m_CanFocus = object.m_CanFocus;
        m_CanTouch = object.m_CanTouch;
        m_CanTraverse = object.m_CanTraverse;
        m_EdgeBehaviorValue = object.m_EdgeBehaviorValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case canFocusPropertyKey:
                m_CanFocus = CoreBoolType::deserialize(reader);
                return true;
            case canTouchPropertyKey:
                m_CanTouch = CoreBoolType::deserialize(reader);
                return true;
            case canTraversePropertyKey:
                m_CanTraverse = CoreBoolType::deserialize(reader);
                return true;
            case edgeBehaviorValuePropertyKey:
                m_EdgeBehaviorValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void canFocusChanged() {}
    virtual void canTouchChanged() {}
    virtual void canTraverseChanged() {}
    virtual void edgeBehaviorValueChanged() {}
};
} // namespace rive

#endif