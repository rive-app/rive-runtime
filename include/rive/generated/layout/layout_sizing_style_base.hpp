#ifndef _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class LayoutSizingStyleBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 1056;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutSizingStyleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t minWidthPropertyKey = 502;
    static const uint16_t maxWidthPropertyKey = 500;
    static const uint16_t minHeightPropertyKey = 503;
    static const uint16_t maxHeightPropertyKey = 501;
    static const uint16_t layoutWidthScaleTypePropertyKey = 655;
    static const uint16_t layoutHeightScaleTypePropertyKey = 656;
    static const uint16_t widthUnitsValuePropertyKey = 607;
    static const uint16_t heightUnitsValuePropertyKey = 608;
    static const uint16_t minWidthUnitsValuePropertyKey = 627;
    static const uint16_t maxWidthUnitsValuePropertyKey = 629;
    static const uint16_t minHeightUnitsValuePropertyKey = 628;
    static const uint16_t maxHeightUnitsValuePropertyKey = 630;
    static const uint16_t justifySelfValuePropertyKey = 1046;
    static const uint16_t displayValuePropertyKey = 596;

protected:
    float m_MinWidth = 0.0f;
    float m_MaxWidth = 0.0f;
    float m_MinHeight = 0.0f;
    float m_MaxHeight = 0.0f;
    uint8_t m_LayoutWidthScaleType = 0;
    uint8_t m_LayoutHeightScaleType = 0;
    uint8_t m_WidthUnitsValue = 1;
    uint8_t m_HeightUnitsValue = 1;
    uint8_t m_MinWidthUnitsValue = 0;
    uint8_t m_MaxWidthUnitsValue = 0;
    uint8_t m_MinHeightUnitsValue = 0;
    uint8_t m_MaxHeightUnitsValue = 0;
    uint8_t m_JustifySelfValue = 6;
    uint8_t m_DisplayValue = 0;

public:
    inline float minWidth() const { return m_MinWidth; }
    void minWidth(float value)
    {
        if (m_MinWidth == value)
        {
            return;
        }
        m_MinWidth = value;
        minWidthChanged();
        notifyPropertyChanged(minWidthPropertyKey);
    }

    inline float maxWidth() const { return m_MaxWidth; }
    void maxWidth(float value)
    {
        if (m_MaxWidth == value)
        {
            return;
        }
        m_MaxWidth = value;
        maxWidthChanged();
        notifyPropertyChanged(maxWidthPropertyKey);
    }

    inline float minHeight() const { return m_MinHeight; }
    void minHeight(float value)
    {
        if (m_MinHeight == value)
        {
            return;
        }
        m_MinHeight = value;
        minHeightChanged();
        notifyPropertyChanged(minHeightPropertyKey);
    }

    inline float maxHeight() const { return m_MaxHeight; }
    void maxHeight(float value)
    {
        if (m_MaxHeight == value)
        {
            return;
        }
        m_MaxHeight = value;
        maxHeightChanged();
        notifyPropertyChanged(maxHeightPropertyKey);
    }

    inline uint8_t layoutWidthScaleType() const
    {
        return m_LayoutWidthScaleType;
    }
    void layoutWidthScaleType(uint8_t value)
    {
        if (m_LayoutWidthScaleType == value)
        {
            return;
        }
        m_LayoutWidthScaleType = value;
        layoutWidthScaleTypeChanged();
        notifyPropertyChanged(layoutWidthScaleTypePropertyKey);
    }

    inline uint8_t layoutHeightScaleType() const
    {
        return m_LayoutHeightScaleType;
    }
    void layoutHeightScaleType(uint8_t value)
    {
        if (m_LayoutHeightScaleType == value)
        {
            return;
        }
        m_LayoutHeightScaleType = value;
        layoutHeightScaleTypeChanged();
        notifyPropertyChanged(layoutHeightScaleTypePropertyKey);
    }

    inline uint8_t widthUnitsValue() const { return m_WidthUnitsValue; }
    void widthUnitsValue(uint8_t value)
    {
        if (m_WidthUnitsValue == value)
        {
            return;
        }
        m_WidthUnitsValue = value;
        widthUnitsValueChanged();
        notifyPropertyChanged(widthUnitsValuePropertyKey);
    }

    inline uint8_t heightUnitsValue() const { return m_HeightUnitsValue; }
    void heightUnitsValue(uint8_t value)
    {
        if (m_HeightUnitsValue == value)
        {
            return;
        }
        m_HeightUnitsValue = value;
        heightUnitsValueChanged();
        notifyPropertyChanged(heightUnitsValuePropertyKey);
    }

    inline uint8_t minWidthUnitsValue() const { return m_MinWidthUnitsValue; }
    void minWidthUnitsValue(uint8_t value)
    {
        if (m_MinWidthUnitsValue == value)
        {
            return;
        }
        m_MinWidthUnitsValue = value;
        minWidthUnitsValueChanged();
        notifyPropertyChanged(minWidthUnitsValuePropertyKey);
    }

    inline uint8_t maxWidthUnitsValue() const { return m_MaxWidthUnitsValue; }
    void maxWidthUnitsValue(uint8_t value)
    {
        if (m_MaxWidthUnitsValue == value)
        {
            return;
        }
        m_MaxWidthUnitsValue = value;
        maxWidthUnitsValueChanged();
        notifyPropertyChanged(maxWidthUnitsValuePropertyKey);
    }

    inline uint8_t minHeightUnitsValue() const { return m_MinHeightUnitsValue; }
    void minHeightUnitsValue(uint8_t value)
    {
        if (m_MinHeightUnitsValue == value)
        {
            return;
        }
        m_MinHeightUnitsValue = value;
        minHeightUnitsValueChanged();
        notifyPropertyChanged(minHeightUnitsValuePropertyKey);
    }

    inline uint8_t maxHeightUnitsValue() const { return m_MaxHeightUnitsValue; }
    void maxHeightUnitsValue(uint8_t value)
    {
        if (m_MaxHeightUnitsValue == value)
        {
            return;
        }
        m_MaxHeightUnitsValue = value;
        maxHeightUnitsValueChanged();
        notifyPropertyChanged(maxHeightUnitsValuePropertyKey);
    }

    inline uint8_t justifySelfValue() const { return m_JustifySelfValue; }
    void justifySelfValue(uint8_t value)
    {
        if (m_JustifySelfValue == value)
        {
            return;
        }
        m_JustifySelfValue = value;
        justifySelfValueChanged();
        notifyPropertyChanged(justifySelfValuePropertyKey);
    }

    inline uint8_t displayValue() const { return m_DisplayValue; }
    void displayValue(uint8_t value)
    {
        if (m_DisplayValue == value)
        {
            return;
        }
        m_DisplayValue = value;
        displayValueChanged();
        notifyPropertyChanged(displayValuePropertyKey);
    }

    void copy(const LayoutSizingStyleBase& object)
    {
        m_MinWidth = object.m_MinWidth;
        m_MaxWidth = object.m_MaxWidth;
        m_MinHeight = object.m_MinHeight;
        m_MaxHeight = object.m_MaxHeight;
        m_LayoutWidthScaleType = object.m_LayoutWidthScaleType;
        m_LayoutHeightScaleType = object.m_LayoutHeightScaleType;
        m_WidthUnitsValue = object.m_WidthUnitsValue;
        m_HeightUnitsValue = object.m_HeightUnitsValue;
        m_MinWidthUnitsValue = object.m_MinWidthUnitsValue;
        m_MaxWidthUnitsValue = object.m_MaxWidthUnitsValue;
        m_MinHeightUnitsValue = object.m_MinHeightUnitsValue;
        m_MaxHeightUnitsValue = object.m_MaxHeightUnitsValue;
        m_JustifySelfValue = object.m_JustifySelfValue;
        m_DisplayValue = object.m_DisplayValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case minWidthPropertyKey:
                m_MinWidth = CoreDoubleType::deserialize(reader);
                return true;
            case maxWidthPropertyKey:
                m_MaxWidth = CoreDoubleType::deserialize(reader);
                return true;
            case minHeightPropertyKey:
                m_MinHeight = CoreDoubleType::deserialize(reader);
                return true;
            case maxHeightPropertyKey:
                m_MaxHeight = CoreDoubleType::deserialize(reader);
                return true;
            case layoutWidthScaleTypePropertyKey:
                m_LayoutWidthScaleType = CoreUintType::deserialize(reader);
                return true;
            case layoutHeightScaleTypePropertyKey:
                m_LayoutHeightScaleType = CoreUintType::deserialize(reader);
                return true;
            case widthUnitsValuePropertyKey:
                m_WidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case heightUnitsValuePropertyKey:
                m_HeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case minWidthUnitsValuePropertyKey:
                m_MinWidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case maxWidthUnitsValuePropertyKey:
                m_MaxWidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case minHeightUnitsValuePropertyKey:
                m_MinHeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case maxHeightUnitsValuePropertyKey:
                m_MaxHeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case justifySelfValuePropertyKey:
                m_JustifySelfValue = CoreUintType::deserialize(reader);
                return true;
            case displayValuePropertyKey:
                m_DisplayValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void minWidthChanged() {}
    virtual void maxWidthChanged() {}
    virtual void minHeightChanged() {}
    virtual void maxHeightChanged() {}
    virtual void layoutWidthScaleTypeChanged() {}
    virtual void layoutHeightScaleTypeChanged() {}
    virtual void widthUnitsValueChanged() {}
    virtual void heightUnitsValueChanged() {}
    virtual void minWidthUnitsValueChanged() {}
    virtual void maxWidthUnitsValueChanged() {}
    virtual void minHeightUnitsValueChanged() {}
    virtual void maxHeightUnitsValueChanged() {}
    virtual void justifySelfValueChanged() {}
    virtual void displayValueChanged() {}
};
} // namespace rive

#endif