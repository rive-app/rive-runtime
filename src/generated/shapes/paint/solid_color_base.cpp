#include "generated/shapes/paint/solid_color_base.hpp"
#include "shapes/paint/solid_color.hpp"

using namespace rive;

Core* SolidColorBase::clone() const
{
	auto cloned = new SolidColor();
	cloned->copy(*this);
	return cloned;
}
