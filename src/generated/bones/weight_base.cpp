#include "generated/bones/weight_base.hpp"
#include "bones/weight.hpp"

using namespace rive;

Core* WeightBase::clone() const
{
	auto cloned = new Weight();
	cloned->copy(*this);
	return cloned;
}
