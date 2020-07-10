#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "generated/drawable_base.hpp"
#include <stdio.h>
namespace rive
{
	class Drawable : public DrawableBase
	{
	public:
		Drawable() { printf("Constructing Drawable\n"); }
	};
} // namespace rive

#endif