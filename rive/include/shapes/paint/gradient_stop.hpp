#ifndef _RIVE_GRADIENT_STOP_HPP_
#define _RIVE_GRADIENT_STOP_HPP_
#include "generated/shapes/paint/gradient_stop_base.hpp"
#include <stdio.h>
namespace rive
{
	class GradientStop : public GradientStopBase
	{
	public:
		GradientStop() { printf("Constructing GradientStop\n"); }
	};
} // namespace rive

#endif