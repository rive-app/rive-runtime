#ifndef _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#define _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScrollConstraintBase : public DraggableConstraint
{
protected:
    typedef DraggableConstraint Super;

public:
    static const uint16_t typeKey = 521;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScrollConstraintBase::typeKey:
            case DraggableConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t scrollOffsetXPropertyKey = 759;
    static const uint16_t scrollOffsetYPropertyKey = 760;
    static const uint16_t scrollPercentXPropertyKey = 761;
    static const uint16_t scrollPercentYPropertyKey = 762;
    static const uint16_t scrollIndexPropertyKey = 763;
    static const uint16_t snapPropertyKey = 724;
    static const uint16_t physicsTypeValuePropertyKey = 727;
    static const uint16_t physicsIdPropertyKey = 726;
    static const uint16_t virtualizePropertyKey = 850;
    static const uint16_t infinitePropertyKey = 851;
    static const uint16_t interactivePropertyKey = 891;
    static const uint16_t thresholdPropertyKey = 894;

protected:
    float m_ScrollOffsetX = 0.0f;
    float m_ScrollOffsetY = 0.0f;
    bool m_Snap = false;
    uint32_t m_PhysicsTypeValue = 0;
    uint32_t m_PhysicsId = -1;
    bool m_Virtualize = false;
    bool m_Infinite = false;
    bool m_Interactive = true;
    float m_Threshold = 0.0f;

public:
    inline float scrollOffsetX() const { return m_ScrollOffsetX; }
    void scrollOffsetX(float value)
    {
        if (m_ScrollOffsetX == value)
        {
            return;
        }
        m_ScrollOffsetX = value;
        scrollOffsetXChanged();
    }

    inline float scrollOffsetY() const { return m_ScrollOffsetY; }
    void scrollOffsetY(float value)
    {
        if (m_ScrollOffsetY == value)
        {
            return;
        }
        m_ScrollOffsetY = value;
        scrollOffsetYChanged();
    }

    virtual void setScrollPercentX(float value) = 0;
    virtual float scrollPercentX() = 0;
    void scrollPercentX(float value)
    {
        if (scrollPercentX() == value)
        {
            return;
        }
        setScrollPercentX(value);
        scrollPercentXChanged();
    }

    virtual void setScrollPercentY(float value) = 0;
    virtual float scrollPercentY() = 0;
    void scrollPercentY(float value)
    {
        if (scrollPercentY() == value)
        {
            return;
        }
        setScrollPercentY(value);
        scrollPercentYChanged();
    }

    virtual void setScrollIndex(float value) = 0;
    virtual float scrollIndex() = 0;
    void scrollIndex(float value)
    {
        if (scrollIndex() == value)
        {
            return;
        }
        setScrollIndex(value);
        scrollIndexChanged();
    }

    inline bool snap() const { return m_Snap; }
    void snap(bool value)
    {
        if (m_Snap == value)
        {
            return;
        }
        m_Snap = value;
        snapChanged();
    }

    inline uint32_t physicsTypeValue() const { return m_PhysicsTypeValue; }
    void physicsTypeValue(uint32_t value)
    {
        if (m_PhysicsTypeValue == value)
        {
            return;
        }
        m_PhysicsTypeValue = value;
        physicsTypeValueChanged();
    }

    inline uint32_t physicsId() const { return m_PhysicsId; }
    void physicsId(uint32_t value)
    {
        if (m_PhysicsId == value)
        {
            return;
        }
        m_PhysicsId = value;
        physicsIdChanged();
    }

    inline bool virtualize() const { return m_Virtualize; }
    void virtualize(bool value)
    {
        if (m_Virtualize == value)
        {
            return;
        }
        m_Virtualize = value;
        virtualizeChanged();
    }

    inline bool infinite() const { return m_Infinite; }
    void infinite(bool value)
    {
        if (m_Infinite == value)
        {
            return;
        }
        m_Infinite = value;
        infiniteChanged();
    }

    inline bool interactive() const { return m_Interactive; }
    void interactive(bool value)
    {
        if (m_Interactive == value)
        {
            return;
        }
        m_Interactive = value;
        interactiveChanged();
    }

    inline float threshold() const { return m_Threshold; }
    void threshold(float value)
    {
        if (m_Threshold == value)
        {
            return;
        }
        m_Threshold = value;
        thresholdChanged();
    }

    Core* clone() const override;
    void copy(const ScrollConstraintBase& object)
    {
        m_ScrollOffsetX = object.m_ScrollOffsetX;
        m_ScrollOffsetY = object.m_ScrollOffsetY;
        m_Snap = object.m_Snap;
        m_PhysicsTypeValue = object.m_PhysicsTypeValue;
        m_PhysicsId = object.m_PhysicsId;
        m_Virtualize = object.m_Virtualize;
        m_Infinite = object.m_Infinite;
        m_Interactive = object.m_Interactive;
        m_Threshold = object.m_Threshold;
        DraggableConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case scrollOffsetXPropertyKey:
                m_ScrollOffsetX = CoreDoubleType::deserialize(reader);
                return true;
            case scrollOffsetYPropertyKey:
                m_ScrollOffsetY = CoreDoubleType::deserialize(reader);
                return true;
            case snapPropertyKey:
                m_Snap = CoreBoolType::deserialize(reader);
                return true;
            case physicsTypeValuePropertyKey:
                m_PhysicsTypeValue = CoreUintType::deserialize(reader);
                return true;
            case physicsIdPropertyKey:
                m_PhysicsId = CoreUintType::deserialize(reader);
                return true;
            case virtualizePropertyKey:
                m_Virtualize = CoreBoolType::deserialize(reader);
                return true;
            case infinitePropertyKey:
                m_Infinite = CoreBoolType::deserialize(reader);
                return true;
            case interactivePropertyKey:
                m_Interactive = CoreBoolType::deserialize(reader);
                return true;
            case thresholdPropertyKey:
                m_Threshold = CoreDoubleType::deserialize(reader);
                return true;
        }
        return DraggableConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void scrollOffsetXChanged() {}
    virtual void scrollOffsetYChanged() {}
    virtual void scrollPercentXChanged() {}
    virtual void scrollPercentYChanged() {}
    virtual void scrollIndexChanged() {}
    virtual void snapChanged() {}
    virtual void physicsTypeValueChanged() {}
    virtual void physicsIdChanged() {}
    virtual void virtualizeChanged() {}
    virtual void infiniteChanged() {}
    virtual void interactiveChanged() {}
    virtual void thresholdChanged() {}
};
} // namespace rive

#endif