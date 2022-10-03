#ifndef _RIVE_SCALE_CONSTRAINT_BASE_HPP_
#define _RIVE_SCALE_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_component_constraint_y.hpp"
namespace rive
{
class ScaleConstraintBase : public TransformComponentConstraintY
{
protected:
    typedef TransformComponentConstraintY Super;

public:
    static const uint16_t typeKey = 88;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScaleConstraintBase::typeKey:
            case TransformComponentConstraintYBase::typeKey:
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