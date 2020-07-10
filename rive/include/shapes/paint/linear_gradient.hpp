#ifndef _RIVE_LINEAR_GRADIENT_HPP_
#define _RIVE_LINEAR_GRADIENT_HPP_
#include "generated/shapes/paint/linear_gradient_base.hpp"
#include <stdio.h>
namespace rive
{
	class LinearGradient : public LinearGradientBase
	{
	public:
		LinearGradient() { printf("Constructing LinearGradient\n"); }
	};
} // namespace rive

#endif