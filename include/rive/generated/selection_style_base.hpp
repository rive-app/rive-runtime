#ifndef _RIVE_SELECTION_STYLE_BASE_HPP_
#define _RIVE_SELECTION_STYLE_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class SelectionStyleBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 153;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SelectionStyleBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t highlightColorPropertyKey = 447;
    static const uint16_t cornerRadiusPropertyKey = 448;

protected:
    int m_HighlightColor = 0x663B82F6;
    float m_CornerRadius = 0.0f;

public:
    inline int highlightColor() const { return m_HighlightColor; }
    void highlightColor(int value)
    {
        if (m_HighlightColor == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(highlightColorPropertyKey,
                             &m_HighlightColor,
                             &value);
        m_HighlightColor = value;
        RIVE_EDITOR_CHANGED(highlightColorChanged());
        notifyPropertyChanged(highlightColorPropertyKey);
    }

    inline float cornerRadius() const { return m_CornerRadius; }
    void cornerRadius(float value)
    {
        if (m_CornerRadius == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusPropertyKey, &m_CornerRadius, &value);
        m_CornerRadius = value;
        RIVE_EDITOR_CHANGED(cornerRadiusChanged());
        notifyPropertyChanged(cornerRadiusPropertyKey);
    }

    Core* clone() const override;
    void copy(const SelectionStyleBase& object)
    {
        m_HighlightColor = object.m_HighlightColor;
        m_CornerRadius = object.m_CornerRadius;
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case highlightColorPropertyKey:
                m_HighlightColor = CoreColorType::deserialize(reader);
                return true;
            case cornerRadiusPropertyKey:
                m_CornerRadius = CoreDoubleType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void highlightColorChanged() {}
    virtual void cornerRadiusChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/selection_style_ext.inl"
#endif
};
} // namespace rive

#endif