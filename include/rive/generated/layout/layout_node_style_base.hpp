#ifndef _RIVE_LAYOUT_NODE_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_NODE_STYLE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/layout/layout_sizing_style.hpp"
namespace rive
{
class LayoutNodeStyleBase : public LayoutSizingStyle
{
protected:
    typedef LayoutSizingStyle Super;

public:
    static const uint16_t typeKey = 1057;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutNodeStyleBase::typeKey:
            case LayoutSizingStyleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t widthPropertyKey = 1066;
    static const uint16_t heightPropertyKey = 1067;
    static const uint16_t fractionalWidthPropertyKey = 1057;
    static const uint16_t fractionalHeightPropertyKey = 1058;

protected:
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    float m_FractionalWidth = 1.0f;
    float m_FractionalHeight = 1.0f;

public:
    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(widthPropertyKey, &m_Width, &value);
        m_Width = value;
        RIVE_EDITOR_CHANGED(widthChanged());
        notifyPropertyChanged(widthPropertyKey);
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(heightPropertyKey, &m_Height, &value);
        m_Height = value;
        RIVE_EDITOR_CHANGED(heightChanged());
        notifyPropertyChanged(heightPropertyKey);
    }

    inline float fractionalWidth() const { return m_FractionalWidth; }
    void fractionalWidth(float value)
    {
        if (m_FractionalWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fractionalWidthPropertyKey,
                             &m_FractionalWidth,
                             &value);
        m_FractionalWidth = value;
        RIVE_EDITOR_CHANGED(fractionalWidthChanged());
        notifyPropertyChanged(fractionalWidthPropertyKey);
    }

    inline float fractionalHeight() const { return m_FractionalHeight; }
    void fractionalHeight(float value)
    {
        if (m_FractionalHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fractionalHeightPropertyKey,
                             &m_FractionalHeight,
                             &value);
        m_FractionalHeight = value;
        RIVE_EDITOR_CHANGED(fractionalHeightChanged());
        notifyPropertyChanged(fractionalHeightPropertyKey);
    }

    Core* clone() const override;
    void copy(const LayoutNodeStyleBase& object)
    {
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_FractionalWidth = object.m_FractionalWidth;
        m_FractionalHeight = object.m_FractionalHeight;
        LayoutSizingStyle::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case fractionalWidthPropertyKey:
                m_FractionalWidth = CoreDoubleType::deserialize(reader);
                return true;
            case fractionalHeightPropertyKey:
                m_FractionalHeight = CoreDoubleType::deserialize(reader);
                return true;
        }
        return LayoutSizingStyle::deserialize(propertyKey, reader);
    }

protected:
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void fractionalWidthChanged() {}
    virtual void fractionalHeightChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/layout_node_style_ext.inl"
#endif
};
} // namespace rive

#endif