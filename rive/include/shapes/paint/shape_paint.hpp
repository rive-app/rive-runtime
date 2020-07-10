#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "generated/shapes/paint/shape_paint_base.hpp"
#include <stdio.h>
namespace rive
{
	class ShapePaint : public ShapePaintBase
	{
	public:
		ShapePaint() { printf("Constructing ShapePaint\n"); }
	};
} // namespace rive

#endif