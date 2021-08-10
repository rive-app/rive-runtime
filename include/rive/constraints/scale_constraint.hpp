#ifndef _RIVE_SCALE_CONSTRAINT_HPP_
#define _RIVE_SCALE_CONSTRAINT_HPP_
#include "rive/generated/constraints/scale_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
	class ScaleConstraint : public ScaleConstraintBase
	{
	public:
		void constrain(TransformComponent* component) override;
	};
} // namespace rive

#endif