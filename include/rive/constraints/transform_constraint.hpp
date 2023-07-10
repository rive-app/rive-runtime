#ifndef _RIVE_TRANSFORM_CONSTRAINT_HPP_
#define _RIVE_TRANSFORM_CONSTRAINT_HPP_
#include "rive/generated/constraints/transform_constraint_base.hpp"
#include "rive/math/transform_components.hpp"

#include <stdio.h>
namespace rive
{
class TransformConstraint : public TransformConstraintBase
{
private:
    TransformComponents m_ComponentsA;
    TransformComponents m_ComponentsB;

public:
    virtual const Mat2D targetTransform() const;
    void constrain(TransformComponent* component) override;
};
} // namespace rive

#endif