#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_
#include "generated/shapes/shape_base.hpp"
#include <stdio.h>
namespace rive
{
	class Shape : public ShapeBase
	{
	public:
		Shape() { printf("Constructing Shape\n"); }
	};
} // namespace rive

#endif