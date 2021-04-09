#ifndef _RIVE_TRANSITION_DOUBLE_CONDITION_HPP_
#define _RIVE_TRANSITION_DOUBLE_CONDITION_HPP_
#include "generated/animation/transition_double_condition_base.hpp"
#include <stdio.h>
namespace rive
{
	class TransitionDoubleCondition : public TransitionDoubleConditionBase
	{
	protected:
		bool validateInputType(const StateMachineInput* input) const override;

	public:
		bool
		evaluate(const StateMachineInputInstance* inputInstance) const override;
	};
} // namespace rive

#endif