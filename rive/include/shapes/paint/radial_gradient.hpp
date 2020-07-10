#ifndef _RIVE_RADIAL_GRADIENT_HPP_
#define _RIVE_RADIAL_GRADIENT_HPP_
#include "generated/shapes/paint/radial_gradient_base.hpp"
#include <stdio.h>
namespace rive
{
	class RadialGradient : public RadialGradientBase
	{
	public:
		RadialGradient() { printf("Constructing RadialGradient\n"); }
	};
} // namespace rive

#endif