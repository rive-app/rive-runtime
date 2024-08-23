#ifndef _RIVE_DISTANCE_CONSTRAINT_HPP_
#define _RIVE_DISTANCE_CONSTRAINT_HPP_
#include "rive/generated/constraints/distance_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class DistanceConstraint : public DistanceConstraintBase
{
public:
    void constrain(TransformComponent* component) override;
    void distanceChanged() override;
    void modeValueChanged() override;
};
} // namespace rive

#endif