#ifndef _RIVE_SHAPE_BASE_HPP_
#define _RIVE_SHAPE_BASE_HPP_
#include "drawable.hpp"
namespace rive
{
	class ShapeBase : public Drawable
	{
	public:
		static const int typeKey = 3;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif