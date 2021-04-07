#ifndef _RIVE_TRANSITION_CONDITION_HPP_
#define _RIVE_TRANSITION_CONDITION_HPP_
#include "generated/animation/transition_condition_base.hpp"
#include <stdio.h>
namespace rive
{
	class TransitionCondition : public TransitionConditionBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;

		StatusCode import(ImportStack& importStack) override;
	};
} // namespace rive

#endif