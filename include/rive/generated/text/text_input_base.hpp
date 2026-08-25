#ifndef _RIVE_TEXT_INPUT_BASE_HPP_
#define _RIVE_TEXT_INPUT_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TextInputBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 569;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextInputBase::typeKey:
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

    static const uint16_t textPropertyKey = 817;
    static const uint16_t selectionRadiusPropertyKey = 818;
    static const uint16_t multilinePropertyKey = 979;

protected:
    std::string m_Text = "";
    float m_SelectionRadius = 5.0f;
    bool m_Multiline = true;

public:
    inline const std::string& text() const { return m_Text; }
    void text(std::string value)
    {
        if (m_Text == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(textPropertyKey, m_Text, value);
        m_Text = value;
        RIVE_EDITOR_CHANGED(textChanged());
        notifyPropertyChanged(textPropertyKey);
    }

    inline float selectionRadius() const { return m_SelectionRadius; }
    void selectionRadius(float value)
    {
        if (m_SelectionRadius == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(selectionRadiusPropertyKey,
                             &m_SelectionRadius,
                             &value);
        m_SelectionRadius = value;
        RIVE_EDITOR_CHANGED(selectionRadiusChanged());
        notifyPropertyChanged(selectionRadiusPropertyKey);
    }

    inline bool multiline() const { return m_Multiline; }
    void multiline(bool value)
    {
        if (m_Multiline == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(multilinePropertyKey, &m_Multiline, &value);
        m_Multiline = value;
        RIVE_EDITOR_CHANGED(multilineChanged());
        notifyPropertyChanged(multilinePropertyKey);
    }

    Core* clone() const override;
    void copy(const TextInputBase& object)
    {
        m_Text = object.m_Text;
        m_SelectionRadius = object.m_SelectionRadius;
        m_Multiline = object.m_Multiline;
        RIVE_EDITOR_COPY(object);
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case textPropertyKey:
                m_Text = CoreStringType::deserialize(reader);
                return true;
            case selectionRadiusPropertyKey:
                m_SelectionRadius = CoreDoubleType::deserialize(reader);
                return true;
            case multilinePropertyKey:
                m_Multiline = CoreBoolType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void textChanged() {}
    virtual void selectionRadiusChanged() {}
    virtual void multilineChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_input_ext.inl"
#endif
};
} // namespace rive

#endif