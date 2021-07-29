#ifndef _RIVE_TRANSFORM_CONSTRAINT_HPP_
#define _RIVE_TRANSFORM_CONSTRAINT_HPP_
#include "generated/constraints/transform_constraint_base.hpp"
#include "transform_space.hpp"
#include "math/transform_components.hpp"

#include <stdio.h>
namespace rive
{
	class TransformConstraint : public TransformConstraintBase
	{
	private:
		TransformComponents m_ComponentsA;
		TransformComponents m_ComponentsB;

	public:
		void constrain(TransformComponent* component) override;
		TransformSpace sourceSpace() const
		{
			return (TransformSpace)sourceSpaceValue();
		}
		TransformSpace destSpace() const
		{
			return (TransformSpace)destSpaceValue();
		}
	};
} // namespace rive

#endif