#ifndef _RIVE_RADIAL_GRADIENT_BASE_HPP_
#define _RIVE_RADIAL_GRADIENT_BASE_HPP_
#include "shapes/paint/linear_gradient.hpp"
namespace rive
{
	class RadialGradientBase : public LinearGradient
	{
	public:
		static const int typeKey = 17;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif