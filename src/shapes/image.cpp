#include "rive/shapes/image.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{
	if (renderOpacity() == 0.0f)
	{
		return;
	}
}