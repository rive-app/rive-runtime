#ifndef _RIVE_ROTATION_CONSTRAINT_HPP_
#define _RIVE_ROTATION_CONSTRAINT_HPP_
#include "rive/generated/constraints/rotation_constraint_base.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{
class RotationConstraint : public RotationConstraintBase
{
private:
    TransformComponents m_ComponentsA;
    TransformComponents m_ComponentsB;

public:
    void constrain(TransformComponent* component) override;
};
} // namespace rive

#endif