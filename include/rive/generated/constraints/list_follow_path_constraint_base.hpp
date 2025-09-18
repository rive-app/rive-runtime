#ifndef _RIVE_LIST_FOLLOW_PATH_CONSTRAINT_BASE_HPP_
#define _RIVE_LIST_FOLLOW_PATH_CONSTRAINT_BASE_HPP_
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class ListFollowPathConstraintBase : public FollowPathConstraint
{
protected:
    typedef FollowPathConstraint Super;

public:
    static const uint16_t typeKey = 625;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListFollowPathConstraintBase::typeKey:
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

    static const uint16_t distanceEndPropertyKey = 888;
    static const uint16_t distanceOffsetPropertyKey = 889;

protected:
    float m_DistanceEnd = 1.0f;
    float m_DistanceOffset = 0.0f;

public:
    inline float distanceEnd() const { return m_DistanceEnd; }
    void distanceEnd(float value)
    {
        if (m_DistanceEnd == value)
        {
            return;
        }
        m_DistanceEnd = value;
        distanceEndChanged();
    }

    inline float distanceOffset() const { return m_DistanceOffset; }
    void distanceOffset(float value)
    {
        if (m_DistanceOffset == value)
        {
            return;
        }
        m_DistanceOffset = value;
        distanceOffsetChanged();
    }

    Core* clone() const override;
    void copy(const ListFollowPathConstraintBase& object)
    {
        m_DistanceEnd = object.m_DistanceEnd;
        m_DistanceOffset = object.m_DistanceOffset;
        FollowPathConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case distanceEndPropertyKey:
                m_DistanceEnd = CoreDoubleType::deserialize(reader);
                return true;
            case distanceOffsetPropertyKey:
                m_DistanceOffset = CoreDoubleType::deserialize(reader);
                return true;
        }
        return FollowPathConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void distanceEndChanged() {}
    virtual void distanceOffsetChanged() {}
};
} // namespace rive

#endif