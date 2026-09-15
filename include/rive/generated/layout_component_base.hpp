#ifndef _RIVE_LAYOUT_COMPONENT_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class LayoutComponentBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 409;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutComponentBase::typeKey:
            case DrawableBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t clipPropertyKey = 196;
    static const uint16_t widthPropertyKey = 7;
    static const uint16_t heightPropertyKey = 8;
    static const uint16_t styleIdPropertyKey = 494;
    static const uint16_t fractionalWidthPropertyKey = 706;
    static const uint16_t fractionalHeightPropertyKey = 707;

protected:
    bool m_Clip = false;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    Id m_StyleId = kEmptyId;
    float m_FractionalWidth = 1.0f;
    float m_FractionalHeight = 1.0f;

public:
    inline bool clip() const { return m_Clip; }
    void clip(bool value)
    {
        if (m_Clip == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(clipPropertyKey, &m_Clip, &value);
        m_Clip = value;
        RIVE_EDITOR_CHANGED(clipChanged());
        notifyPropertyChanged(clipPropertyKey);
    }

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

    inline Id styleId() const { return m_StyleId; }
    void styleId(Id value)
    {
        if (m_StyleId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(styleIdPropertyKey, &m_StyleId, &value);
        m_StyleId = value;
        RIVE_EDITOR_CHANGED(styleIdChanged());
        notifyPropertyChanged(styleIdPropertyKey);
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
    void copy(const LayoutComponentBase& object)
    {
        m_Clip = object.m_Clip;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_StyleId = object.m_StyleId;
        m_FractionalWidth = object.m_FractionalWidth;
        m_FractionalHeight = object.m_FractionalHeight;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case clipPropertyKey:
                m_Clip = CoreBoolType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case styleIdPropertyKey:
                m_StyleId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case fractionalWidthPropertyKey:
                m_FractionalWidth = CoreDoubleType::deserialize(reader);
                return true;
            case fractionalHeightPropertyKey:
                m_FractionalHeight = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void clipChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void styleIdChanged() {}
    virtual void fractionalWidthChanged() {}
    virtual void fractionalHeightChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout_component_ext.inl"
#endif
};
} // namespace rive

#endif