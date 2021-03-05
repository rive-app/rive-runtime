#ifndef _RIVE_STATE_MACHINE_BASE_HPP_
#define _RIVE_STATE_MACHINE_BASE_HPP_
#include "animation/animation.hpp"
namespace rive
{
	class StateMachineBase : public Animation
	{
	protected:
		typedef Animation Super;

	public:
		static const int typeKey = 53;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineBase::typeKey:
				case AnimationBase::typeKey:
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