#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_BASE_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_BASE_HPP_
#include "animation/keyframe.hpp"
namespace rive
{
	class KeyFrameDrawOrderBase : public KeyFrame
	{
	protected:
		typedef KeyFrame Super;

	public:
		static const int typeKey = 32;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case KeyFrameDrawOrderBase::typeKey:
				case KeyFrameBase::typeKey:
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