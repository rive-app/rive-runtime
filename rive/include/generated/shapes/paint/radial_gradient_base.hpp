#ifndef _RIVE_RADIAL_GRADIENT_BASE_HPP_
#define _RIVE_RADIAL_GRADIENT_BASE_HPP_
#include "shapes/paint/linear_gradient.hpp"
namespace rive
{
	class RadialGradientBase : public LinearGradient
	{
	protected:
		typedef LinearGradient Super;

	public:
		static const int typeKey = 17;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case RadialGradientBase::typeKey:
				case LinearGradientBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
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