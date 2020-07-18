#ifndef _RIVE_SHAPE_BASE_HPP_
#define _RIVE_SHAPE_BASE_HPP_
#include "drawable.hpp"
namespace rive
{
	class ShapeBase : public Drawable
	{
	public:
		static const int typeKey = 3;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case ShapeBase::typeKey:
				case DrawableBase::typeKey:
				case NodeBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif