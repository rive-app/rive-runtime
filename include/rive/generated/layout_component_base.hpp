#ifndef _RIVE_LAYOUT_COMPONENT_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class LayoutComponentBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 409;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    bool m_Clip = true;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    uint32_t m_StyleId = -1;

public:
    inline bool clip() const { return m_Clip; }
    void clip(bool value)
    {
        if (m_Clip == value)
        {
            return;
        }
        m_Clip = value;
        clipChanged();
    }

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        m_Width = value;
        widthChanged();
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        m_Height = value;
        heightChanged();
    }

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

    Core* clone() const override;
    void copy(const LayoutComponentBase& object)
    {
        m_Clip = object.m_Clip;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_StyleId = object.m_StyleId;
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
                m_StyleId = CoreUintType::deserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void clipChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void styleIdChanged() {}
};
} // namespace rive

#endif