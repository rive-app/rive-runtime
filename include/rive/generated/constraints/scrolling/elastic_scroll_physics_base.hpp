#ifndef _RIVE_ELASTIC_SCROLL_PHYSICS_BASE_HPP_
#define _RIVE_ELASTIC_SCROLL_PHYSICS_BASE_HPP_
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class ElasticScrollPhysicsBase : public ScrollPhysics
{
protected:
    typedef ScrollPhysics Super;

public:
    static const uint16_t typeKey = 525;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ElasticScrollPhysicsBase::typeKey:
            case ScrollPhysicsBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t frictionPropertyKey = 728;
    static const uint16_t speedMultiplierPropertyKey = 729;
    static const uint16_t elasticFactorPropertyKey = 730;

protected:
    float m_Friction = 8.0f;
    float m_SpeedMultiplier = 1.0f;
    float m_ElasticFactor = 0.66f;

public:
    inline float friction() const { return m_Friction; }
    void friction(float value)
    {
        if (m_Friction == value)
        {
            return;
        }
        m_Friction = value;
        frictionChanged();
    }

    inline float speedMultiplier() const { return m_SpeedMultiplier; }
    void speedMultiplier(float value)
    {
        if (m_SpeedMultiplier == value)
        {
            return;
        }
        m_SpeedMultiplier = value;
        speedMultiplierChanged();
    }

    inline float elasticFactor() const { return m_ElasticFactor; }
    void elasticFactor(float value)
    {
        if (m_ElasticFactor == value)
        {
            return;
        }
        m_ElasticFactor = value;
        elasticFactorChanged();
    }

    Core* clone() const override;
    void copy(const ElasticScrollPhysicsBase& object)
    {
        m_Friction = object.m_Friction;
        m_SpeedMultiplier = object.m_SpeedMultiplier;
        m_ElasticFactor = object.m_ElasticFactor;
        ScrollPhysics::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case frictionPropertyKey:
                m_Friction = CoreDoubleType::deserialize(reader);
                return true;
            case speedMultiplierPropertyKey:
                m_SpeedMultiplier = CoreDoubleType::deserialize(reader);
                return true;
            case elasticFactorPropertyKey:
                m_ElasticFactor = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ScrollPhysics::deserialize(propertyKey, reader);
    }

protected:
    virtual void frictionChanged() {}
    virtual void speedMultiplierChanged() {}
    virtual void elasticFactorChanged() {}
};
} // namespace rive

#endif