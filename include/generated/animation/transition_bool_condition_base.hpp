#ifndef _RIVE_TRANSITION_BOOL_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_BOOL_CONDITION_BASE_HPP_
#include "animation/transition_value_condition.hpp"
namespace rive
{
	class TransitionBoolConditionBase : public TransitionValueCondition
	{
	protected:
		typedef TransitionValueCondition Super;

	public:
		static const int typeKey = 71;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case TransitionBoolConditionBase::typeKey:
				case TransitionValueConditionBase::typeKey:
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