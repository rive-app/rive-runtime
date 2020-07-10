#ifndef _RIVE_CUBIC_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_HPP_
#include "generated/animation/cubic_interpolator_base.hpp"
#include <stdio.h>
namespace rive
{
	class CubicInterpolator : public CubicInterpolatorBase
	{
	public:
		CubicInterpolator() { printf("Constructing CubicInterpolator\n"); }
	};
} // namespace rive

#endif