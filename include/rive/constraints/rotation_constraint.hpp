#ifndef _RIVE_ROTATION_CONSTRAINT_HPP_
#define _RIVE_ROTATION_CONSTRAINT_HPP_
#include "rive/generated/constraints/rotation_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
	class RotationConstraint : public RotationConstraintBase
	{
	public:
		void constrain(TransformComponent* component) override;
	};
} // namespace rive

#endif