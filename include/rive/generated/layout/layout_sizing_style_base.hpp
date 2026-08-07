#ifndef _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/sidecar.hpp"
namespace rive
{
struct LayoutSizingStyleMinMaxSizingSidecar
{
    float minWidth = 0.0f;
    float maxWidth = 0.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    uint8_t minWidthUnitsValue = 0;
    uint8_t maxWidthUnitsValue = 0;
    uint8_t minHeightUnitsValue = 0;
    uint8_t maxHeightUnitsValue = 0;
};
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
    static const uint16_t minWidthUnitsValuePropertyKey = 627;
    static const uint16_t maxWidthUnitsValuePropertyKey = 629;
    static const uint16_t minHeightUnitsValuePropertyKey = 628;
    static const uint16_t maxHeightUnitsValuePropertyKey = 630;
    static const uint16_t layoutWidthScaleTypePropertyKey = 655;
    static const uint16_t layoutHeightScaleTypePropertyKey = 656;
    static const uint16_t widthUnitsValuePropertyKey = 607;
    static const uint16_t heightUnitsValuePropertyKey = 608;
    static const uint16_t justifySelfValuePropertyKey = 1046;
    static const uint16_t displayValuePropertyKey = 596;

protected:
    uint8_t m_LayoutWidthScaleType = 0;
    uint8_t m_LayoutHeightScaleType = 0;
    uint8_t m_WidthUnitsValue = 1;
    uint8_t m_HeightUnitsValue = 1;
    uint8_t m_JustifySelfValue = 6;
    uint8_t m_DisplayValue = 0;
    Sidecar<LayoutSizingStyleMinMaxSizingSidecar> m_minMaxSizing;

public:
    inline float minWidth() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->minWidth : 0.0f;
    }
    void minWidth(float value)
    {
        if (minWidth() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->minWidth = value;
        minWidthChanged();
        notifyPropertyChanged(minWidthPropertyKey);
    }

    inline float maxWidth() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->maxWidth : 0.0f;
    }
    void maxWidth(float value)
    {
        if (maxWidth() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->maxWidth = value;
        maxWidthChanged();
        notifyPropertyChanged(maxWidthPropertyKey);
    }

    inline float minHeight() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->minHeight : 0.0f;
    }
    void minHeight(float value)
    {
        if (minHeight() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->minHeight = value;
        minHeightChanged();
        notifyPropertyChanged(minHeightPropertyKey);
    }

    inline float maxHeight() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->maxHeight : 0.0f;
    }
    void maxHeight(float value)
    {
        if (maxHeight() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->maxHeight = value;
        maxHeightChanged();
        notifyPropertyChanged(maxHeightPropertyKey);
    }

    inline uint8_t minWidthUnitsValue() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->minWidthUnitsValue : 0;
    }
    void minWidthUnitsValue(uint8_t value)
    {
        if (minWidthUnitsValue() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->minWidthUnitsValue = value;
        minWidthUnitsValueChanged();
        notifyPropertyChanged(minWidthUnitsValuePropertyKey);
    }

    inline uint8_t maxWidthUnitsValue() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->maxWidthUnitsValue : 0;
    }
    void maxWidthUnitsValue(uint8_t value)
    {
        if (maxWidthUnitsValue() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->maxWidthUnitsValue = value;
        maxWidthUnitsValueChanged();
        notifyPropertyChanged(maxWidthUnitsValuePropertyKey);
    }

    inline uint8_t minHeightUnitsValue() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->minHeightUnitsValue : 0;
    }
    void minHeightUnitsValue(uint8_t value)
    {
        if (minHeightUnitsValue() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->minHeightUnitsValue = value;
        minHeightUnitsValueChanged();
        notifyPropertyChanged(minHeightUnitsValuePropertyKey);
    }

    inline uint8_t maxHeightUnitsValue() const
    {
        auto* sidecar = m_minMaxSizing.get();
        return sidecar != nullptr ? sidecar->maxHeightUnitsValue : 0;
    }
    void maxHeightUnitsValue(uint8_t value)
    {
        if (maxHeightUnitsValue() == value)
        {
            return;
        }
        m_minMaxSizing.ensure()->maxHeightUnitsValue = value;
        maxHeightUnitsValueChanged();
        notifyPropertyChanged(maxHeightUnitsValuePropertyKey);
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
        m_LayoutWidthScaleType = object.m_LayoutWidthScaleType;
        m_LayoutHeightScaleType = object.m_LayoutHeightScaleType;
        m_WidthUnitsValue = object.m_WidthUnitsValue;
        m_HeightUnitsValue = object.m_HeightUnitsValue;
        m_JustifySelfValue = object.m_JustifySelfValue;
        m_DisplayValue = object.m_DisplayValue;
        m_minMaxSizing = object.m_minMaxSizing;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case minWidthPropertyKey:
                m_minMaxSizing.ensure()->minWidth =
                    CoreDoubleType::deserialize(reader);
                return true;
            case maxWidthPropertyKey:
                m_minMaxSizing.ensure()->maxWidth =
                    CoreDoubleType::deserialize(reader);
                return true;
            case minHeightPropertyKey:
                m_minMaxSizing.ensure()->minHeight =
                    CoreDoubleType::deserialize(reader);
                return true;
            case maxHeightPropertyKey:
                m_minMaxSizing.ensure()->maxHeight =
                    CoreDoubleType::deserialize(reader);
                return true;
            case minWidthUnitsValuePropertyKey:
                m_minMaxSizing.ensure()->minWidthUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case maxWidthUnitsValuePropertyKey:
                m_minMaxSizing.ensure()->maxWidthUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case minHeightUnitsValuePropertyKey:
                m_minMaxSizing.ensure()->minHeightUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case maxHeightUnitsValuePropertyKey:
                m_minMaxSizing.ensure()->maxHeightUnitsValue =
                    CoreUintType::deserialize(reader);
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
    virtual void minWidthUnitsValueChanged() {}
    virtual void maxWidthUnitsValueChanged() {}
    virtual void minHeightUnitsValueChanged() {}
    virtual void maxHeightUnitsValueChanged() {}
    virtual void layoutWidthScaleTypeChanged() {}
    virtual void layoutHeightScaleTypeChanged() {}
    virtual void widthUnitsValueChanged() {}
    virtual void heightUnitsValueChanged() {}
    virtual void justifySelfValueChanged() {}
    virtual void displayValueChanged() {}
};
} // namespace rive

#endif