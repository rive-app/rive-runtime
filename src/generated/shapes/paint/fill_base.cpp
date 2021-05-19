#include "generated/shapes/paint/fill_base.hpp"
#include "shapes/paint/fill.hpp"

using namespace rive;

Core* FillBase::clone() const
{
	auto cloned = new Fill();
	cloned->copy(*this);
	return cloned;
}
