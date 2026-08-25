#ifndef _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_SIZING_STYLE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
#ifndef WITH_RIVE_EDITOR
#include "rive/sidecar.hpp"
#endif
namespace rive
{
#ifndef WITH_RIVE_EDITOR
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
#endif
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
#ifdef WITH_RIVE_EDITOR
    float m_MinWidth = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_MaxWidth = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_MinHeight = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_MaxHeight = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_MinWidthUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_MaxWidthUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_MinHeightUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_MaxHeightUnitsValue = 0;
#endif
    uint8_t m_LayoutWidthScaleType = 0;
    uint8_t m_LayoutHeightScaleType = 0;
    uint8_t m_WidthUnitsValue = 1;
    uint8_t m_HeightUnitsValue = 1;
    uint8_t m_JustifySelfValue = 6;
    uint8_t m_DisplayValue = 0;
#ifndef WITH_RIVE_EDITOR
    Sidecar<LayoutSizingStyleMinMaxSizingSidecar> m_minMaxSizing;
#endif
public:
#ifdef WITH_RIVE_EDITOR
    inline float minWidth() const { return m_MinWidth; }
    void minWidth(float value)
    {
        if (m_MinWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minWidthPropertyKey, &m_MinWidth, &value);
        m_MinWidth = value;
        RIVE_EDITOR_CHANGED(minWidthChanged());
        notifyPropertyChanged(minWidthPropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->minWidth = value;
        minWidthChanged();
        notifyPropertyChanged(minWidthPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float maxWidth() const { return m_MaxWidth; }
    void maxWidth(float value)
    {
        if (m_MaxWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxWidthPropertyKey, &m_MaxWidth, &value);
        m_MaxWidth = value;
        RIVE_EDITOR_CHANGED(maxWidthChanged());
        notifyPropertyChanged(maxWidthPropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->maxWidth = value;
        maxWidthChanged();
        notifyPropertyChanged(maxWidthPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float minHeight() const { return m_MinHeight; }
    void minHeight(float value)
    {
        if (m_MinHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minHeightPropertyKey, &m_MinHeight, &value);
        m_MinHeight = value;
        RIVE_EDITOR_CHANGED(minHeightChanged());
        notifyPropertyChanged(minHeightPropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->minHeight = value;
        minHeightChanged();
        notifyPropertyChanged(minHeightPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float maxHeight() const { return m_MaxHeight; }
    void maxHeight(float value)
    {
        if (m_MaxHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxHeightPropertyKey, &m_MaxHeight, &value);
        m_MaxHeight = value;
        RIVE_EDITOR_CHANGED(maxHeightChanged());
        notifyPropertyChanged(maxHeightPropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->maxHeight = value;
        maxHeightChanged();
        notifyPropertyChanged(maxHeightPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t minWidthUnitsValue() const { return m_MinWidthUnitsValue; }
    void minWidthUnitsValue(uint8_t value)
    {
        if (m_MinWidthUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minWidthUnitsValuePropertyKey,
                             &m_MinWidthUnitsValue,
                             &value);
        m_MinWidthUnitsValue = value;
        RIVE_EDITOR_CHANGED(minWidthUnitsValueChanged());
        notifyPropertyChanged(minWidthUnitsValuePropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->minWidthUnitsValue = value;
        minWidthUnitsValueChanged();
        notifyPropertyChanged(minWidthUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t maxWidthUnitsValue() const { return m_MaxWidthUnitsValue; }
    void maxWidthUnitsValue(uint8_t value)
    {
        if (m_MaxWidthUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxWidthUnitsValuePropertyKey,
                             &m_MaxWidthUnitsValue,
                             &value);
        m_MaxWidthUnitsValue = value;
        RIVE_EDITOR_CHANGED(maxWidthUnitsValueChanged());
        notifyPropertyChanged(maxWidthUnitsValuePropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->maxWidthUnitsValue = value;
        maxWidthUnitsValueChanged();
        notifyPropertyChanged(maxWidthUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t minHeightUnitsValue() const { return m_MinHeightUnitsValue; }
    void minHeightUnitsValue(uint8_t value)
    {
        if (m_MinHeightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(minHeightUnitsValuePropertyKey,
                             &m_MinHeightUnitsValue,
                             &value);
        m_MinHeightUnitsValue = value;
        RIVE_EDITOR_CHANGED(minHeightUnitsValueChanged());
        notifyPropertyChanged(minHeightUnitsValuePropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->minHeightUnitsValue = value;
        minHeightUnitsValueChanged();
        notifyPropertyChanged(minHeightUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t maxHeightUnitsValue() const { return m_MaxHeightUnitsValue; }
    void maxHeightUnitsValue(uint8_t value)
    {
        if (m_MaxHeightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(maxHeightUnitsValuePropertyKey,
                             &m_MaxHeightUnitsValue,
                             &value);
        m_MaxHeightUnitsValue = value;
        RIVE_EDITOR_CHANGED(maxHeightUnitsValueChanged());
        notifyPropertyChanged(maxHeightUnitsValuePropertyKey);
    }
#else
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
        m_minMaxSizing.ensureAllocated()->maxHeightUnitsValue = value;
        maxHeightUnitsValueChanged();
        notifyPropertyChanged(maxHeightUnitsValuePropertyKey);
    }
#endif

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
        RIVE_EDITOR_CHANGING(layoutWidthScaleTypePropertyKey,
                             &m_LayoutWidthScaleType,
                             &value);
        m_LayoutWidthScaleType = value;
        RIVE_EDITOR_CHANGED(layoutWidthScaleTypeChanged());
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
        RIVE_EDITOR_CHANGING(layoutHeightScaleTypePropertyKey,
                             &m_LayoutHeightScaleType,
                             &value);
        m_LayoutHeightScaleType = value;
        RIVE_EDITOR_CHANGED(layoutHeightScaleTypeChanged());
        notifyPropertyChanged(layoutHeightScaleTypePropertyKey);
    }

    inline uint8_t widthUnitsValue() const { return m_WidthUnitsValue; }
    void widthUnitsValue(uint8_t value)
    {
        if (m_WidthUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(widthUnitsValuePropertyKey,
                             &m_WidthUnitsValue,
                             &value);
        m_WidthUnitsValue = value;
        RIVE_EDITOR_CHANGED(widthUnitsValueChanged());
        notifyPropertyChanged(widthUnitsValuePropertyKey);
    }

    inline uint8_t heightUnitsValue() const { return m_HeightUnitsValue; }
    void heightUnitsValue(uint8_t value)
    {
        if (m_HeightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(heightUnitsValuePropertyKey,
                             &m_HeightUnitsValue,
                             &value);
        m_HeightUnitsValue = value;
        RIVE_EDITOR_CHANGED(heightUnitsValueChanged());
        notifyPropertyChanged(heightUnitsValuePropertyKey);
    }

    inline uint8_t justifySelfValue() const { return m_JustifySelfValue; }
    void justifySelfValue(uint8_t value)
    {
        if (m_JustifySelfValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(justifySelfValuePropertyKey,
                             &m_JustifySelfValue,
                             &value);
        m_JustifySelfValue = value;
        RIVE_EDITOR_CHANGED(justifySelfValueChanged());
        notifyPropertyChanged(justifySelfValuePropertyKey);
    }

    inline uint8_t displayValue() const { return m_DisplayValue; }
    void displayValue(uint8_t value)
    {
        if (m_DisplayValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(displayValuePropertyKey, &m_DisplayValue, &value);
        m_DisplayValue = value;
        RIVE_EDITOR_CHANGED(displayValueChanged());
        notifyPropertyChanged(displayValuePropertyKey);
    }

    void copy(const LayoutSizingStyleBase& object)
    {
#ifdef WITH_RIVE_EDITOR
        m_MinWidth = object.m_MinWidth;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MaxWidth = object.m_MaxWidth;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MinHeight = object.m_MinHeight;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MaxHeight = object.m_MaxHeight;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MinWidthUnitsValue = object.m_MinWidthUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MaxWidthUnitsValue = object.m_MaxWidthUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MinHeightUnitsValue = object.m_MinHeightUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_MaxHeightUnitsValue = object.m_MaxHeightUnitsValue;
#endif
        m_LayoutWidthScaleType = object.m_LayoutWidthScaleType;
        m_LayoutHeightScaleType = object.m_LayoutHeightScaleType;
        m_WidthUnitsValue = object.m_WidthUnitsValue;
        m_HeightUnitsValue = object.m_HeightUnitsValue;
        m_JustifySelfValue = object.m_JustifySelfValue;
        m_DisplayValue = object.m_DisplayValue;
#ifndef WITH_RIVE_EDITOR
        m_minMaxSizing = object.m_minMaxSizing;
#endif
        RIVE_EDITOR_COPY(object);
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case minWidthPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MinWidth = CoreDoubleType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->minWidth =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case maxWidthPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MaxWidth = CoreDoubleType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->maxWidth =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case minHeightPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MinHeight = CoreDoubleType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->minHeight =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case maxHeightPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MaxHeight = CoreDoubleType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->maxHeight =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case minWidthUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MinWidthUnitsValue = CoreUintType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->minWidthUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case maxWidthUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MaxWidthUnitsValue = CoreUintType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->maxWidthUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case minHeightUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MinHeightUnitsValue = CoreUintType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->minHeightUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case maxHeightUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_MaxHeightUnitsValue = CoreUintType::deserialize(reader);
#else
                m_minMaxSizing.ensureAllocated()->maxHeightUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
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
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/layout_sizing_style_ext.inl"
#endif
};
} // namespace rive

#endif