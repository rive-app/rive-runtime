#ifndef _RIVE_FOLLOW_PATH_CONSTRAINT_BASE_HPP_
#define _RIVE_FOLLOW_PATH_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_space_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class FollowPathConstraintBase : public TransformSpaceConstraint
{
protected:
    typedef TransformSpaceConstraint Super;

public:
    static const uint16_t typeKey = 165;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FollowPathConstraintBase::typeKey:
            case TransformSpaceConstraintBase::typeKey:
            case TargetedConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t distancePropertyKey = 363;
    static const uint16_t orientPropertyKey = 364;
    static const uint16_t offsetPropertyKey = 365;

private:
    float m_Distance = 0.0f;
    bool m_Orient = true;
    bool m_Offset = false;

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

    inline bool orient() const { return m_Orient; }
    void orient(bool value)
    {
        if (m_Orient == value)
        {
            return;
        }
        m_Orient = value;
        orientChanged();
    }

    inline bool offset() const { return m_Offset; }
    void offset(bool value)
    {
        if (m_Offset == value)
        {
            return;
        }
        m_Offset = value;
        offsetChanged();
    }

    Core* clone() const override;
    void copy(const FollowPathConstraintBase& object)
    {
        m_Distance = object.m_Distance;
        m_Orient = object.m_Orient;
        m_Offset = object.m_Offset;
        TransformSpaceConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case distancePropertyKey:
                m_Distance = CoreDoubleType::deserialize(reader);
                return true;
            case orientPropertyKey:
                m_Orient = CoreBoolType::deserialize(reader);
                return true;
            case offsetPropertyKey:
                m_Offset = CoreBoolType::deserialize(reader);
                return true;
        }
        return TransformSpaceConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void distanceChanged() {}
    virtual void orientChanged() {}
    virtual void offsetChanged() {}
};
} // namespace rive

#endif