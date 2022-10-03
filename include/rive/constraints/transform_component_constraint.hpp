#ifndef _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_HPP_
#define _RIVE_TRANSFORM_COMPONENT_CONSTRAINT_HPP_
#include "rive/generated/constraints/transform_component_constraint_base.hpp"
#include "rive/transform_space.hpp"
#include <stdio.h>
namespace rive
{
class TransformComponentConstraint : public TransformComponentConstraintBase
{
public:
    TransformSpace minMaxSpace() const { return (TransformSpace)minMaxSpaceValue(); }
};
} // namespace rive

#endif