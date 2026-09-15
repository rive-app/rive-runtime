#ifndef _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#define _RIVE_SCROLL_CONSTRAINT_BASE_HPP_
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
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
    static const uint16_t virtualizeBufferPropertyKey = 221;
    static const uint16_t infinitePropertyKey = 851;
    static const uint16_t interactivePropertyKey = 891;
    static const uint16_t thresholdPropertyKey = 894;
    static const uint16_t velocityXPropertyKey = 1023;
    static const uint16_t velocityYPropertyKey = 1024;
    static const uint16_t scrollActivePropertyKey = 1025;
    static const uint16_t dragMultiplierPropertyKey = 1029;
    static const uint16_t computedContentWidthPropertyKey = 1069;
    static const uint16_t computedContentHeightPropertyKey = 1070;

protected:
    float m_ScrollOffsetX = 0.0f;
    float m_ScrollOffsetY = 0.0f;
    bool m_Snap = false;
    uint32_t m_PhysicsTypeValue = 0;
    Id m_PhysicsId = kEmptyId;
    bool m_Virtualize = false;
    uint8_t m_VirtualizeBuffer = 0;
    bool m_Infinite = false;
    bool m_Interactive = true;
    float m_Threshold = 0.0f;
    float m_DragMultiplier = 1.0f;

public:
    inline float scrollOffsetX() const { return m_ScrollOffsetX; }
    void scrollOffsetX(float value)
    {
        if (m_ScrollOffsetX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(scrollOffsetXPropertyKey,
                             &m_ScrollOffsetX,
                             &value);
        m_ScrollOffsetX = value;
        RIVE_EDITOR_CHANGED(scrollOffsetXChanged());
        notifyPropertyChanged(scrollOffsetXPropertyKey);
    }

    inline float scrollOffsetY() const { return m_ScrollOffsetY; }
    void scrollOffsetY(float value)
    {
        if (m_ScrollOffsetY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(scrollOffsetYPropertyKey,
                             &m_ScrollOffsetY,
                             &value);
        m_ScrollOffsetY = value;
        RIVE_EDITOR_CHANGED(scrollOffsetYChanged());
        notifyPropertyChanged(scrollOffsetYPropertyKey);
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
        notifyPropertyChanged(scrollPercentXPropertyKey);
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
        notifyPropertyChanged(scrollPercentYPropertyKey);
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
        notifyPropertyChanged(scrollIndexPropertyKey);
    }

    inline bool snap() const { return m_Snap; }
    void snap(bool value)
    {
        if (m_Snap == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(snapPropertyKey, &m_Snap, &value);
        m_Snap = value;
        RIVE_EDITOR_CHANGED(snapChanged());
        notifyPropertyChanged(snapPropertyKey);
    }

    inline uint32_t physicsTypeValue() const { return m_PhysicsTypeValue; }
    void physicsTypeValue(uint32_t value)
    {
        if (m_PhysicsTypeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(physicsTypeValuePropertyKey,
                             &m_PhysicsTypeValue,
                             &value);
        m_PhysicsTypeValue = value;
        RIVE_EDITOR_CHANGED(physicsTypeValueChanged());
        notifyPropertyChanged(physicsTypeValuePropertyKey);
    }

    inline Id physicsId() const { return m_PhysicsId; }
    void physicsId(Id value)
    {
        if (m_PhysicsId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(physicsIdPropertyKey, &m_PhysicsId, &value);
        m_PhysicsId = value;
        RIVE_EDITOR_CHANGED(physicsIdChanged());
        notifyPropertyChanged(physicsIdPropertyKey);
    }

    inline bool virtualize() const { return m_Virtualize; }
    void virtualize(bool value)
    {
        if (m_Virtualize == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(virtualizePropertyKey, &m_Virtualize, &value);
        m_Virtualize = value;
        RIVE_EDITOR_CHANGED(virtualizeChanged());
        notifyPropertyChanged(virtualizePropertyKey);
    }

    inline uint8_t virtualizeBuffer() const { return m_VirtualizeBuffer; }
    void virtualizeBuffer(uint8_t value)
    {
        if (m_VirtualizeBuffer == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(virtualizeBufferPropertyKey,
                             &m_VirtualizeBuffer,
                             &value);
        m_VirtualizeBuffer = value;
        RIVE_EDITOR_CHANGED(virtualizeBufferChanged());
        notifyPropertyChanged(virtualizeBufferPropertyKey);
    }

    inline bool infinite() const { return m_Infinite; }
    void infinite(bool value)
    {
        if (m_Infinite == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(infinitePropertyKey, &m_Infinite, &value);
        m_Infinite = value;
        RIVE_EDITOR_CHANGED(infiniteChanged());
        notifyPropertyChanged(infinitePropertyKey);
    }

    inline bool interactive() const { return m_Interactive; }
    void interactive(bool value)
    {
        if (m_Interactive == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interactivePropertyKey, &m_Interactive, &value);
        m_Interactive = value;
        RIVE_EDITOR_CHANGED(interactiveChanged());
        notifyPropertyChanged(interactivePropertyKey);
    }

    inline float threshold() const { return m_Threshold; }
    void threshold(float value)
    {
        if (m_Threshold == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(thresholdPropertyKey, &m_Threshold, &value);
        m_Threshold = value;
        RIVE_EDITOR_CHANGED(thresholdChanged());
        notifyPropertyChanged(thresholdPropertyKey);
    }

    virtual void setVelocityX(float value) = 0;
    virtual float velocityX() = 0;
    void velocityX(float value)
    {
        if (velocityX() == value)
        {
            return;
        }
        setVelocityX(value);
        velocityXChanged();
        notifyPropertyChanged(velocityXPropertyKey);
    }

    virtual void setVelocityY(float value) = 0;
    virtual float velocityY() = 0;
    void velocityY(float value)
    {
        if (velocityY() == value)
        {
            return;
        }
        setVelocityY(value);
        velocityYChanged();
        notifyPropertyChanged(velocityYPropertyKey);
    }

    virtual void setScrollActive(bool value) = 0;
    virtual bool scrollActive() = 0;
    void scrollActive(bool value)
    {
        if (scrollActive() == value)
        {
            return;
        }
        setScrollActive(value);
        scrollActiveChanged();
        notifyPropertyChanged(scrollActivePropertyKey);
    }

    inline float dragMultiplier() const { return m_DragMultiplier; }
    void dragMultiplier(float value)
    {
        if (m_DragMultiplier == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(dragMultiplierPropertyKey,
                             &m_DragMultiplier,
                             &value);
        m_DragMultiplier = value;
        RIVE_EDITOR_CHANGED(dragMultiplierChanged());
        notifyPropertyChanged(dragMultiplierPropertyKey);
    }

    virtual void setComputedContentWidth(float value) = 0;
    virtual float computedContentWidth() = 0;
    void computedContentWidth(float value)
    {
        if (computedContentWidth() == value)
        {
            return;
        }
        setComputedContentWidth(value);
        computedContentWidthChanged();
        notifyPropertyChanged(computedContentWidthPropertyKey);
    }

    virtual void setComputedContentHeight(float value) = 0;
    virtual float computedContentHeight() = 0;
    void computedContentHeight(float value)
    {
        if (computedContentHeight() == value)
        {
            return;
        }
        setComputedContentHeight(value);
        computedContentHeightChanged();
        notifyPropertyChanged(computedContentHeightPropertyKey);
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
        m_VirtualizeBuffer = object.m_VirtualizeBuffer;
        m_Infinite = object.m_Infinite;
        m_Interactive = object.m_Interactive;
        m_Threshold = object.m_Threshold;
        m_DragMultiplier = object.m_DragMultiplier;
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
                m_PhysicsId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case virtualizePropertyKey:
                m_Virtualize = CoreBoolType::deserialize(reader);
                return true;
            case virtualizeBufferPropertyKey:
                m_VirtualizeBuffer = CoreUintType::deserialize(reader);
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
            case dragMultiplierPropertyKey:
                m_DragMultiplier = CoreDoubleType::deserialize(reader);
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
    virtual void virtualizeBufferChanged() {}
    virtual void infiniteChanged() {}
    virtual void interactiveChanged() {}
    virtual void thresholdChanged() {}
    virtual void velocityXChanged() {}
    virtual void velocityYChanged() {}
    virtual void scrollActiveChanged() {}
    virtual void dragMultiplierChanged() {}
    virtual void computedContentWidthChanged() {}
    virtual void computedContentHeightChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/constraints/scrolling/scroll_constraint_ext.inl"
#endif
};
} // namespace rive

#endif