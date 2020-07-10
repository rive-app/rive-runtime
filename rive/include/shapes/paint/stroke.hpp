#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "generated/shapes/paint/stroke_base.hpp"
#include <stdio.h>
namespace rive
{
	class Stroke : public StrokeBase
	{
	public:
		Stroke() { printf("Constructing Stroke\n"); }
	};
} // namespace rive

#endif