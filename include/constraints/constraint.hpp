#ifndef _RIVE_CONSTRAINT_HPP_
#define _RIVE_CONSTRAINT_HPP_
#include "generated/constraints/constraint_base.hpp"
#include <stdio.h>
namespace rive
{
	class TransformComponent;
	class Constraint : public ConstraintBase
	{
	public:
		void strengthChanged() override;
		StatusCode onAddedClean(CoreContext* context) override;
		virtual void markConstraintDirty();
		virtual void constrain(TransformComponent* component) = 0;
		void buildDependencies() override;
	};
} // namespace rive

#endif