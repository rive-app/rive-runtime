#ifndef _RIVE_IMAGE_HPP_
#define _RIVE_IMAGE_HPP_
#include "rive/generated/shapes/image_base.hpp"
#include <stdio.h>
namespace rive
{
	class Image : public ImageBase
	{
	public:
		void draw(Renderer* renderer) override;
	};
} // namespace rive

#endif