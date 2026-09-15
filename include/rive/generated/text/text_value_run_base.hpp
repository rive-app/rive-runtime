#ifndef _RIVE_TEXT_VALUE_RUN_BASE_HPP_
#define _RIVE_TEXT_VALUE_RUN_BASE_HPP_
#include <string>
#include "rive/component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TextValueRunBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 135;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextValueRunBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t styleIdPropertyKey = 272;
    static const uint16_t textPropertyKey = 268;

protected:
    Id m_StyleId = kEmptyId;
    std::string m_Text = "";

public:
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

    Core* clone() const override;
    void copy(const TextValueRunBase& object)
    {
        m_StyleId = object.m_StyleId;
        m_Text = object.m_Text;
        RIVE_EDITOR_COPY(object);
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case styleIdPropertyKey:
                m_StyleId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case textPropertyKey:
                m_Text = CoreStringType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void styleIdChanged() {}
    virtual void textChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_value_run_ext.inl"
#endif
};
} // namespace rive

#endif