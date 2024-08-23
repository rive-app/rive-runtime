#ifndef _RIVE_SCALE_CONSTRAINT_HPP_
#define _RIVE_SCALE_CONSTRAINT_HPP_
#include "rive/generated/constraints/scale_constraint_base.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{
class ScaleConstraint : public ScaleConstraintBase
{
private:
    TransformComponents m_ComponentsA;
    TransformComponents m_ComponentsB;

public:
    void constrain(TransformComponent* component) override;
};
} // namespace rive

#endif