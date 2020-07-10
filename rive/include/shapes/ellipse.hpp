#ifndef _RIVE_ELLIPSE_HPP_
#define _RIVE_ELLIPSE_HPP_
#include "generated/shapes/ellipse_base.hpp"
#include <stdio.h>
namespace rive
{
	class Ellipse : public EllipseBase
	{
	public:
		Ellipse() { printf("Constructing Ellipse\n"); }
	};
} // namespace rive

#endif