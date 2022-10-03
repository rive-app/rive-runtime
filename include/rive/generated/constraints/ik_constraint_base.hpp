#ifndef _RIVE_I_KCONSTRAINT_BASE_HPP_
#define _RIVE_I_KCONSTRAINT_BASE_HPP_
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class IKConstraintBase : public TargetedConstraint
{
protected:
    typedef TargetedConstraint Super;

public:
    static const uint16_t typeKey = 81;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case IKConstraintBase::typeKey:
            case TargetedConstraintBase::typeKey:
            case ConstraintBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t invertDirectionPropertyKey = 174;
    static const uint16_t parentBoneCountPropertyKey = 175;

private:
    bool m_InvertDirection = false;
    uint32_t m_ParentBoneCount = 0;

public:
    inline bool invertDirection() const { return m_InvertDirection; }
    void invertDirection(bool value)
    {
        if (m_InvertDirection == value)
        {
            return;
        }
        m_InvertDirection = value;
        invertDirectionChanged();
    }

    inline uint32_t parentBoneCount() const { return m_ParentBoneCount; }
    void parentBoneCount(uint32_t value)
    {
        if (m_ParentBoneCount == value)
        {
            return;
        }
        m_ParentBoneCount = value;
        parentBoneCountChanged();
    }

    Core* clone() const override;
    void copy(const IKConstraintBase& object)
    {
        m_InvertDirection = object.m_InvertDirection;
        m_ParentBoneCount = object.m_ParentBoneCount;
        TargetedConstraint::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case invertDirectionPropertyKey:
                m_InvertDirection = CoreBoolType::deserialize(reader);
                return true;
            case parentBoneCountPropertyKey:
                m_ParentBoneCount = CoreUintType::deserialize(reader);
                return true;
        }
        return TargetedConstraint::deserialize(propertyKey, reader);
    }

protected:
    virtual void invertDirectionChanged() {}
    virtual void parentBoneCountChanged() {}
};
} // namespace rive

#endif