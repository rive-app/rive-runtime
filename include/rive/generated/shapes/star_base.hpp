#ifndef _RIVE_STAR_BASE_HPP_
#define _RIVE_STAR_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/shapes/polygon.hpp"
namespace rive
{
class StarBase : public Polygon
{
protected:
    typedef Polygon Super;

public:
    static const uint16_t typeKey = 52;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StarBase::typeKey:
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

    static const uint16_t innerRadiusPropertyKey = 127;

private:
    float m_InnerRadius = 0.5f;

public:
    inline float innerRadius() const { return m_InnerRadius; }
    void innerRadius(float value)
    {
        if (m_InnerRadius == value)
        {
            return;
        }
        m_InnerRadius = value;
        innerRadiusChanged();
    }

    Core* clone() const override;
    void copy(const StarBase& object)
    {
        m_InnerRadius = object.m_InnerRadius;
        Polygon::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case innerRadiusPropertyKey:
                m_InnerRadius = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Polygon::deserialize(propertyKey, reader);
    }

protected:
    virtual void innerRadiusChanged() {}
};
} // namespace rive

#endif