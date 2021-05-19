#include "generated/backboard_base.hpp"
#include "backboard.hpp"

using namespace rive;

Core* BackboardBase::clone() const
{
	auto cloned = new Backboard();
	cloned->copy(*this);
	return cloned;
}
