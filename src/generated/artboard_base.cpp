#include "generated/artboard_base.hpp"
#include "artboard.hpp"

using namespace rive;

Core* ArtboardBase::clone() const
{
	auto cloned = new Artboard();
	cloned->copy(*this);
	return cloned;
}
