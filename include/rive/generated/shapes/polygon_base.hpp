#ifndef _RIVE_POLYGON_BASE_HPP_
#define _RIVE_POLYGON_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/shapes/parametric_path.hpp"
namespace rive
{
class PolygonBase : public ParametricPath
{
protected:
    typedef ParametricPath Super;

public:
    static const uint16_t typeKey = 51;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case PolygonBase::typeKey:
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

    static const uint16_t pointsPropertyKey = 125;
    static const uint16_t cornerRadiusPropertyKey = 126;

private:
    uint32_t m_Points = 5;
    float m_CornerRadius = 0.0f;

public:
    inline uint32_t points() const { return m_Points; }
    void points(uint32_t value)
    {
        if (m_Points == value)
        {
            return;
        }
        m_Points = value;
        pointsChanged();
    }

    inline float cornerRadius() const { return m_CornerRadius; }
    void cornerRadius(float value)
    {
        if (m_CornerRadius == value)
        {
            return;
        }
        m_CornerRadius = value;
        cornerRadiusChanged();
    }

    Core* clone() const override;
    void copy(const PolygonBase& object)
    {
        m_Points = object.m_Points;
        m_CornerRadius = object.m_CornerRadius;
        ParametricPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case pointsPropertyKey:
                m_Points = CoreUintType::deserialize(reader);
                return true;
            case cornerRadiusPropertyKey:
                m_CornerRadius = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ParametricPath::deserialize(propertyKey, reader);
    }

protected:
    virtual void pointsChanged() {}
    virtual void cornerRadiusChanged() {}
};
} // namespace rive

#endif