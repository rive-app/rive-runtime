#ifndef _RIVE_TRANSITION_TRIGGER_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_TRIGGER_CONDITION_BASE_HPP_
#include "animation/transition_condition.hpp"
namespace rive
{
	class TransitionTriggerConditionBase : public TransitionCondition
	{
	protected:
		typedef TransitionCondition Super;

	public:
		static const int typeKey = 68;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case TransitionTriggerConditionBase::typeKey:
				case TransitionConditionBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

	protected:
	};
} // namespace rive

#endif