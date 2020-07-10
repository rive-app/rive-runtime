#ifndef _RIVE_RECTANGLE_HPP_
#define _RIVE_RECTANGLE_HPP_
#include "generated/shapes/rectangle_base.hpp"
#include <stdio.h>
namespace rive
{
	class Rectangle : public RectangleBase
	{
	public:
		Rectangle() { printf("Constructing Rectangle\n"); }
	};
} // namespace rive

#endif