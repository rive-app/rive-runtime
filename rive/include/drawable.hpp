#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "generated/drawable_base.hpp"
#include "renderer.hpp"

namespace rive
{
	class Drawable : public DrawableBase
	{
	public:
		virtual void draw(Renderer* renderer) = 0;
	};
} // namespace rive

#endif