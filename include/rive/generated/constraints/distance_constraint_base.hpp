#ifndef _RIVE_DISTANCE_CONSTRAINT_BASE_HPP_
#define _RIVE_DISTANCE_CONSTRAINT_BASE_HPP_
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class DistanceConstraintBase : public TargetedConstraint
{
protected:
    typedef TargetedConstraint Super;

public:
    static const uint16_t typeKey = 82;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DistanceConstraintBase::typeKey:
            case TargetedConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t distancePropertyKey = 177;
    static const uint16_t modeValuePropertyKey = 178;

private:
    float m_Distance = 100.0f;
    uint32_t m_ModeValue = 0;

public:
    inline float distance() const { return m_Distance; }
    void distance(float value)
    {
        if (m_Distance == value)
        {
            return;
        }
        m_Distance = value;
        distanceChanged();
    }

    inline uint32_t modeValue() const { return m_ModeValue; }
    void modeValue(uint32_t value)
    {
        if (m_ModeValue == value)
        {
            return;
        }
        m_ModeValue = value;
        modeValueChanged();
    }

    Core* clone() const override;
    void copy(const DistanceConstraintBase& object)
    {
        m_Distance = object.m_Distance;
        m_ModeValue = object.m_ModeValue;
        TargetedConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case distancePropertyKey:
                m_Distance = CoreDoubleType::deserialize(reader);
                return true;
            case modeValuePropertyKey:
                m_ModeValue = CoreUintType::deserialize(reader);
                return true;
        }
        return TargetedConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void distanceChanged() {}
    virtual void modeValueChanged() {}
};
} // namespace rive

#endif