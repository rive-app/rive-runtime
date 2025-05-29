#ifndef _RIVE_TEXT_INPUT_BASE_HPP_
#define _RIVE_TEXT_INPUT_BASE_HPP_
#include <string>
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/drawable.hpp"
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

protected:
    std::string m_Text = "";
    float m_SelectionRadius = 5.0f;

public:
    inline const std::string& text() const { return m_Text; }
    void text(std::string value)
    {
        if (m_Text == value)
        {
            return;
        }
        m_Text = value;
        textChanged();
    }

    inline float selectionRadius() const { return m_SelectionRadius; }
    void selectionRadius(float value)
    {
        if (m_SelectionRadius == value)
        {
            return;
        }
        m_SelectionRadius = value;
        selectionRadiusChanged();
    }

    Core* clone() const override;
    void copy(const TextInputBase& object)
    {
        m_Text = object.m_Text;
        m_SelectionRadius = object.m_SelectionRadius;
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
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void textChanged() {}
    virtual void selectionRadiusChanged() {}
};
} // namespace rive

#endif