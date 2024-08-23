#ifndef _RIVE_TEXT_VALUE_RUN_BASE_HPP_
#define _RIVE_TEXT_VALUE_RUN_BASE_HPP_
#include <string>
#include "rive/component.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextValueRunBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 135;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_StyleId = -1;
    std::string m_Text = "";

public:
    inline uint32_t styleId() const { return m_StyleId; }
    void styleId(uint32_t value)
    {
        if (m_StyleId == value)
        {
            return;
        }
        m_StyleId = value;
        styleIdChanged();
    }

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

    Core* clone() const override;
    void copy(const TextValueRunBase& object)
    {
        m_StyleId = object.m_StyleId;
        m_Text = object.m_Text;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case styleIdPropertyKey:
                m_StyleId = CoreUintType::deserialize(reader);
                return true;
            case textPropertyKey:
                m_Text = CoreStringType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void styleIdChanged() {}
    virtual void textChanged() {}
};
} // namespace rive

#endif