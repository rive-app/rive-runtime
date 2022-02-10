#include "rive/generated/constraints/transform_constraint_base.hpp"
#include "rive/constraints/transform_constraint.hpp"

using namespace rive;

Core* TransformConstraintBase::clone() const
{
    auto cloned = new TransformConstraint();
    cloned->copy(*this);
    return cloned;
}
