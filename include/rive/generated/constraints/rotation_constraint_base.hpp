#ifndef _RIVE_ROTATION_CONSTRAINT_BASE_HPP_
#define _RIVE_ROTATION_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_component_constraint.hpp"
namespace rive
{
class RotationConstraintBase : public TransformComponentConstraint
{
protected:
    typedef TransformComponentConstraint Super;

public:
    static const uint16_t typeKey = 89;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case RotationConstraintBase::typeKey:
            case TransformComponentConstraintBase::typeKey:
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

    Core* clone() const override;

protected:
};
} // namespace rive

#endif