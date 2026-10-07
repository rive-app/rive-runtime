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

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif