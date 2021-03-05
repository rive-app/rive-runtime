#ifndef _RIVE_STATE_MACHINE_TRIGGER_BASE_HPP_
#define _RIVE_STATE_MACHINE_TRIGGER_BASE_HPP_
#include "animation/state_machine_input.hpp"
namespace rive
{
	class StateMachineTriggerBase : public StateMachineInput
	{
	protected:
		typedef StateMachineInput Super;

	public:
		static const int typeKey = 58;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineTriggerBase::typeKey:
				case StateMachineInputBase::typeKey:
				case StateMachineComponentBase::typeKey:
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