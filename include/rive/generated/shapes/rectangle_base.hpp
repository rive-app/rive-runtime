#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/parametric_path.hpp"
#include "rive/sidecar.hpp"
namespace rive
{
struct RectangleCornerRadiusSidecar
{
    bool linkCornerRadius = true;
    float cornerRadiusTL = 0.0f;
    float cornerRadiusTR = 0.0f;
    float cornerRadiusBL = 0.0f;
    float cornerRadiusBR = 0.0f;
};
class RectangleBase : public ParametricPath
{
protected:
    typedef ParametricPath Super;

public:
    static const uint16_t typeKey = 7;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Sidecar<RectangleCornerRadiusSidecar> m_cornerRadius;

public:
    inline bool linkCornerRadius() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->linkCornerRadius : true;
    }
    void linkCornerRadius(bool value)
    {
        if (linkCornerRadius() == value)
        {
            return;
        }
        m_cornerRadius.ensureAllocated()->linkCornerRadius = value;
        linkCornerRadiusChanged();
        notifyPropertyChanged(linkCornerRadiusPropertyKey);
    }

    inline float cornerRadiusTL() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusTL : 0.0f;
    }
    void cornerRadiusTL(float value)
    {
        if (cornerRadiusTL() == value)
        {
            return;
        }
        m_cornerRadius.ensureAllocated()->cornerRadiusTL = value;
        cornerRadiusTLChanged();
        notifyPropertyChanged(cornerRadiusTLPropertyKey);
    }

    inline float cornerRadiusTR() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusTR : 0.0f;
    }
    void cornerRadiusTR(float value)
    {
        if (cornerRadiusTR() == value)
        {
            return;
        }
        m_cornerRadius.ensureAllocated()->cornerRadiusTR = value;
        cornerRadiusTRChanged();
        notifyPropertyChanged(cornerRadiusTRPropertyKey);
    }

    inline float cornerRadiusBL() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusBL : 0.0f;
    }
    void cornerRadiusBL(float value)
    {
        if (cornerRadiusBL() == value)
        {
            return;
        }
        m_cornerRadius.ensureAllocated()->cornerRadiusBL = value;
        cornerRadiusBLChanged();
        notifyPropertyChanged(cornerRadiusBLPropertyKey);
    }

    inline float cornerRadiusBR() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusBR : 0.0f;
    }
    void cornerRadiusBR(float value)
    {
        if (cornerRadiusBR() == value)
        {
            return;
        }
        m_cornerRadius.ensureAllocated()->cornerRadiusBR = value;
        cornerRadiusBRChanged();
        notifyPropertyChanged(cornerRadiusBRPropertyKey);
    }

    Core* clone() const override;
    void copy(const RectangleBase& object)
    {
        m_cornerRadius = object.m_cornerRadius;
        ParametricPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case linkCornerRadiusPropertyKey:
                m_cornerRadius.ensureAllocated()->linkCornerRadius =
                    CoreBoolType::deserialize(reader);
                return true;
            case cornerRadiusTLPropertyKey:
                m_cornerRadius.ensureAllocated()->cornerRadiusTL =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusTRPropertyKey:
                m_cornerRadius.ensureAllocated()->cornerRadiusTR =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBLPropertyKey:
                m_cornerRadius.ensureAllocated()->cornerRadiusBL =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBRPropertyKey:
                m_cornerRadius.ensureAllocated()->cornerRadiusBR =
                    CoreDoubleType::deserialize(reader);
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