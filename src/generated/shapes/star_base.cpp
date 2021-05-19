#include "generated/shapes/star_base.hpp"
#include "shapes/star.hpp"

using namespace rive;

Core* StarBase::clone() const
{
	auto cloned = new Star();
	cloned->copy(*this);
	return cloned;
}
