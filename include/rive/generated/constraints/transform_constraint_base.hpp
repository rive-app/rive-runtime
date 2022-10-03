#ifndef _RIVE_TRANSFORM_CONSTRAINT_BASE_HPP_
#define _RIVE_TRANSFORM_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_space_constraint.hpp"
namespace rive
{
class TransformConstraintBase : public TransformSpaceConstraint
{
protected:
    typedef TransformSpaceConstraint Super;

public:
    static const uint16_t typeKey = 83;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransformConstraintBase::typeKey:
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