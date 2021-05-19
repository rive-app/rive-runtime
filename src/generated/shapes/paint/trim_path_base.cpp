#include "generated/shapes/paint/trim_path_base.hpp"
#include "shapes/paint/trim_path.hpp"

using namespace rive;

Core* TrimPathBase::clone() const
{
	auto cloned = new TrimPath();
	cloned->copy(*this);
	return cloned;
}
