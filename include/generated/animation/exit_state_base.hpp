#ifndef _RIVE_EXIT_STATE_BASE_HPP_
#define _RIVE_EXIT_STATE_BASE_HPP_
#include "animation/layer_state.hpp"
namespace rive
{
	class ExitStateBase : public LayerState
	{
	protected:
		typedef LayerState Super;

	public:
		static const int typeKey = 64;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case ExitStateBase::typeKey:
				case LayerStateBase::typeKey:
				case StateMachineLayerComponentBase::typeKey:
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