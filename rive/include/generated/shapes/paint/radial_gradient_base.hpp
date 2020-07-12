#ifndef _RIVE_RADIAL_GRADIENT_BASE_HPP_
#define _RIVE_RADIAL_GRADIENT_BASE_HPP_
#include "shapes/paint/linear_gradient.hpp"
namespace rive
{
	class RadialGradientBase : public LinearGradient
	{
	public:
		static const int typeKey = 17;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool inheritsFrom(int typeKey) override
		{
			switch (typeKey)
			{
				case LinearGradientBase::typeKey:
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