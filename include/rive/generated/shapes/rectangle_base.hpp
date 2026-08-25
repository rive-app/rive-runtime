#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/parametric_path.hpp"
#ifndef WITH_RIVE_EDITOR
#include "rive/sidecar.hpp"
#endif
namespace rive
{
#ifndef WITH_RIVE_EDITOR
struct RectangleCornerRadiusSidecar
{
    bool linkCornerRadius = true;
    float cornerRadiusTL = 0.0f;
    float cornerRadiusTR = 0.0f;
    float cornerRadiusBL = 0.0f;
    float cornerRadiusBR = 0.0f;
};
#endif
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
#ifdef WITH_RIVE_EDITOR
    bool m_LinkCornerRadius = true;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusTL = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusTR = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusBL = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusBR = 0.0f;
#endif
#ifndef WITH_RIVE_EDITOR
    Sidecar<RectangleCornerRadiusSidecar> m_cornerRadius;
#endif
public:
#ifdef WITH_RIVE_EDITOR
    inline bool linkCornerRadius() const { return m_LinkCornerRadius; }
    void linkCornerRadius(bool value)
    {
        if (m_LinkCornerRadius == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(linkCornerRadiusPropertyKey,
                             &m_LinkCornerRadius,
                             &value);
        m_LinkCornerRadius = value;
        RIVE_EDITOR_CHANGED(linkCornerRadiusChanged());
        notifyPropertyChanged(linkCornerRadiusPropertyKey);
    }
#else
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
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusTL() const { return m_CornerRadiusTL; }
    void cornerRadiusTL(float value)
    {
        if (m_CornerRadiusTL == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusTLPropertyKey,
                             &m_CornerRadiusTL,
                             &value);
        m_CornerRadiusTL = value;
        RIVE_EDITOR_CHANGED(cornerRadiusTLChanged());
        notifyPropertyChanged(cornerRadiusTLPropertyKey);
    }
#else
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
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusTR() const { return m_CornerRadiusTR; }
    void cornerRadiusTR(float value)
    {
        if (m_CornerRadiusTR == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusTRPropertyKey,
                             &m_CornerRadiusTR,
                             &value);
        m_CornerRadiusTR = value;
        RIVE_EDITOR_CHANGED(cornerRadiusTRChanged());
        notifyPropertyChanged(cornerRadiusTRPropertyKey);
    }
#else
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
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusBL() const { return m_CornerRadiusBL; }
    void cornerRadiusBL(float value)
    {
        if (m_CornerRadiusBL == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusBLPropertyKey,
                             &m_CornerRadiusBL,
                             &value);
        m_CornerRadiusBL = value;
        RIVE_EDITOR_CHANGED(cornerRadiusBLChanged());
        notifyPropertyChanged(cornerRadiusBLPropertyKey);
    }
#else
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
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusBR() const { return m_CornerRadiusBR; }
    void cornerRadiusBR(float value)
    {
        if (m_CornerRadiusBR == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusBRPropertyKey,
                             &m_CornerRadiusBR,
                             &value);
        m_CornerRadiusBR = value;
        RIVE_EDITOR_CHANGED(cornerRadiusBRChanged());
        notifyPropertyChanged(cornerRadiusBRPropertyKey);
    }
#else
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
#endif

    Core* clone() const override;
    void copy(const RectangleBase& object)
    {
#ifdef WITH_RIVE_EDITOR
        m_LinkCornerRadius = object.m_LinkCornerRadius;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusTL = object.m_CornerRadiusTL;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusTR = object.m_CornerRadiusTR;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusBL = object.m_CornerRadiusBL;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusBR = object.m_CornerRadiusBR;
#endif
#ifndef WITH_RIVE_EDITOR
        m_cornerRadius = object.m_cornerRadius;
#endif
        ParametricPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case linkCornerRadiusPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_LinkCornerRadius = CoreBoolType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->linkCornerRadius =
                    CoreBoolType::deserialize(reader);
#endif
                return true;
            case cornerRadiusTLPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusTL = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusTL =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusTRPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusTR = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusTR =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusBLPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusBL = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusBL =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusBRPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusBR = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusBR =
                    CoreDoubleType::deserialize(reader);
#endif
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/rectangle_ext.inl"
#endif
};
} // namespace rive

#endif