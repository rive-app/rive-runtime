#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/parametric_path.hpp"
namespace rive
{
class RectangleBase : public ParametricPath
{
protected:
    typedef ParametricPath Super;

public:
    static const uint16_t typeKey = 7;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case RectangleBase::typeKey:
            case ParametricPathBase::typeKey:
            case PathBase::typeKey:
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

    static const uint16_t linkCornerRadiusPropertyKey = 164;
    static const uint16_t cornerRadiusTLPropertyKey = 31;
    static const uint16_t cornerRadiusTRPropertyKey = 161;
    static const uint16_t cornerRadiusBLPropertyKey = 162;
    static const uint16_t cornerRadiusBRPropertyKey = 163;

private:
    bool m_LinkCornerRadius = true;
    float m_CornerRadiusTL = 0.0f;
    float m_CornerRadiusTR = 0.0f;
    float m_CornerRadiusBL = 0.0f;
    float m_CornerRadiusBR = 0.0f;

public:
    inline bool linkCornerRadius() const { return m_LinkCornerRadius; }
    void linkCornerRadius(bool value)
    {
        if (m_LinkCornerRadius == value)
        {
            return;
        }
        m_LinkCornerRadius = value;
        linkCornerRadiusChanged();
    }

    inline float cornerRadiusTL() const { return m_CornerRadiusTL; }
    void cornerRadiusTL(float value)
    {
        if (m_CornerRadiusTL == value)
        {
            return;
        }
        m_CornerRadiusTL = value;
        cornerRadiusTLChanged();
    }

    inline float cornerRadiusTR() const { return m_CornerRadiusTR; }
    void cornerRadiusTR(float value)
    {
        if (m_CornerRadiusTR == value)
        {
            return;
        }
        m_CornerRadiusTR = value;
        cornerRadiusTRChanged();
    }

    inline float cornerRadiusBL() const { return m_CornerRadiusBL; }
    void cornerRadiusBL(float value)
    {
        if (m_CornerRadiusBL == value)
        {
            return;
        }
        m_CornerRadiusBL = value;
        cornerRadiusBLChanged();
    }

    inline float cornerRadiusBR() const { return m_CornerRadiusBR; }
    void cornerRadiusBR(float value)
    {
        if (m_CornerRadiusBR == value)
        {
            return;
        }
        m_CornerRadiusBR = value;
        cornerRadiusBRChanged();
    }

    Core* clone() const override;
    void copy(const RectangleBase& object)
    {
        m_LinkCornerRadius = object.m_LinkCornerRadius;
        m_CornerRadiusTL = object.m_CornerRadiusTL;
        m_CornerRadiusTR = object.m_CornerRadiusTR;
        m_CornerRadiusBL = object.m_CornerRadiusBL;
        m_CornerRadiusBR = object.m_CornerRadiusBR;
        ParametricPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case linkCornerRadiusPropertyKey:
                m_LinkCornerRadius = CoreBoolType::deserialize(reader);
                return true;
            case cornerRadiusTLPropertyKey:
                m_CornerRadiusTL = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusTRPropertyKey:
                m_CornerRadiusTR = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBLPropertyKey:
                m_CornerRadiusBL = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBRPropertyKey:
                m_CornerRadiusBR = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ParametricPath::deserialize(propertyKey, reader);
    }

protected:
    virtual void linkCornerRadiusChanged() {}
    virtual void cornerRadiusTLChanged() {}
    virtual void cornerRadiusTRChanged() {}
    virtual void cornerRadiusBLChanged() {}
    virtual void cornerRadiusBRChanged() {}
};
} // namespace rive

#endif